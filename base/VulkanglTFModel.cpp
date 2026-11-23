#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE

#include "VulkanglTFModel.h"

VkDescriptorSetLayout vkglTF::descriptorSetLayoutImage = VK_NULL_HANDLE;
VkDescriptorSetLayout vkglTF::descriptorSetLayoutUbo = VK_NULL_HANDLE;
VkMemoryPropertyFlags vkglTF::memoryPropertyFlags = 0;
uint32_t vkglTF::descriptorBindingFlags = vkglTF::DescriptorBindingFlags::ImageBaseColor;


bool loadImageDataFunc(tinygltf::Image* image, const int imageIndex, std::string* error, std::string* warning, int req_width, int req_height, const unsigned char* bytes, int size, void* userData)
{
	if(image->uri.find_last_of(".") != std::string::npos)
	{
		if(image->uri.substr(image->uri.find_last_of(".") + 1) == "ktx")
		{
			return true;
		}
	}

	return tinygltf::LoadImageData(image, imageIndex, error, warning, req_width, req_height, bytes, size, userData);
}

// used by samples that dont required images to be loaded
bool loadImageDataFuncEmpty(tinygltf::Image* image, const int imageIndex, std::string* error, std::string* warning, int req_width, int req_height, const unsigned char* bytes, int size, void* userData)
{
	return true;
}

namespace vkglTF
{
	/// gltf Texutres

	void Texture::updateDescriptor()
	{
		descriptor.sampler = sampler;
		descriptor.imageView = view;
		descriptor.imageLayout = imageLayout;
	}

	void Texture::destroy()
	{
		if(device)
		{
			vkDestroyImageView(device->logicalDevice, view, nullptr);
			vkDestroyImage(device->logicalDevice, image, nullptr);
			vkFreeMemory(device->logicalDevice, deviceMemory, nullptr);
			vkDestroySampler(device->logicalDevice, sampler, nullptr);
		}
	}

	void Texture::fromgltfImage(tinygltf::Image& gltfImage, std::string path, vkt::VulkanDevice* device, VkQueue copyQueue)
	{
		this->device = device;

		bool isKtx = false;
		
		if (gltfImage.uri.find_last_of(".") != std::string::npos)
		{
			if (gltfImage.uri.substr(gltfImage.uri.find_last_of(".") + 1) == "ktx")
			{
				isKtx = true;
			}
		}

		VkFormat format;

		if(!isKtx)
		{
			// load using stb image

			unsigned char* buffer;
			VkDeviceSize bufferSize = 0;
			bool deleteBuffer = false;
			if(gltfImage.component = 3)
			{
				// most devices dont support rgb only on vulkan, check actual format support and trasnfrorm if required
				bufferSize = gltfImage.width * gltfImage.height * 4;
				buffer = new unsigned char[bufferSize];
				unsigned char* rgba = buffer;
				unsigned char* rgb = &gltfImage.image[0];
				for(size_t i = 0; i < gltfImage.width * gltfImage.height; ++i)
				{
					for(int32_t j = 0; j < 3; ++j)
					{
						rgba[j] = rgb[j];
					}
					rgba += 4;
					rgb += 3;
				}
				deleteBuffer = true;
			}
			else
			{
				buffer = &gltfImage.image[0];
				bufferSize = gltfImage.image.size();
			}
			assert(buffer);

			format = VK_FORMAT_R8G8B8A8_UNORM;

			width = gltfImage.width;
			height = gltfImage.height;
			mipLevels = static_cast<uint32_t>(floor(log2(std::max(width, height))) + 1.0);

			VkFormatProperties formatProperties;
			vkGetPhysicalDeviceFormatProperties(device->physicalDevice, format, &formatProperties);
			assert(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT);
			assert(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT);

			VkBuffer stagingBuffer;
			VkDeviceMemory stagingMemory;

			VkBufferCreateInfo bufferCreateInfo
			{
				.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.size = bufferSize,
				.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
			};
			VK_CHECK_RESULT(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));
			VkMemoryRequirements memoryReqs{};
			vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memoryReqs);
			VkMemoryAllocateInfo memoryAllocInfo
			{
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.allocationSize = memoryReqs.size,
				.memoryTypeIndex = device->getMemoryType(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
			};

			VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memoryAllocInfo, nullptr, &stagingMemory));
			VK_CHECK_RESULT(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

			uint32_t* data{ nullptr };
			VK_CHECK_RESULT(vkMapMemory(device->logicalDevice, stagingMemory, 0, memoryReqs.size, 0, (void**)data));
			memcpy(data, buffer, bufferSize);
			vkUnmapMemory(device->logicalDevice, stagingMemory);

			VkImageCreateInfo imageCreateInfo
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = format,
				.extent = { .width = width, .height = height, .depth = 1},
				.mipLevels = mipLevels,
				.arrayLayers = 1,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.tiling = VK_IMAGE_TILING_OPTIMAL,
				.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
			};

			VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));
			vkGetImageMemoryRequirements(device->logicalDevice, image, &memoryReqs);
			memoryAllocInfo.allocationSize = memoryReqs.size;
			memoryAllocInfo.memoryTypeIndex = device->getMemoryType(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memoryAllocInfo, nullptr, &deviceMemory));
			VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));

			VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
			VkImageSubresourceRange subresourceRange
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = 1,
				.layerCount = 1
			};

			{
				VkImageMemoryBarrier imageMemoryBarrier
				{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = 0,
					.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					.image = image,
					.subresourceRange = subresourceRange
				};
				vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
			}

			VkBufferImageCopy bufferCopyRegion
			{
				.imageSubresource =
				{
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = 0,
					.baseArrayLayer = 0,
					.layerCount = 1
				},
				.imageExtent =
				{
					.width = width,
					.height = height,
					.depth = 1
				}
			};
			vkCmdCopyBufferToImage(copyCmd, stagingBuffer, image, imageLayout, 1, &bufferCopyRegion);
			{
				VkImageMemoryBarrier imageMemoryBarrier
				{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					.image = image,
					.subresourceRange = subresourceRange
				};
				vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
			}
			device->flushCommandBuffer(copyCmd, copyQueue, true);
			
			vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);
			vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);

			// generate mip chain
			VkCommandBuffer blitCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
			for(uint32_t i = 0; i < mipLevels; i++)
			{
				VkImageBlit imageBlit{};
				imageBlit.srcSubresource = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = i - 1,
				.layerCount = 1,
				};
				imageBlit.srcOffsets[1] = {
					.x = int32_t(width >> (i - 1)),
					.y = int32_t(height >> (i - 1)),
					.z = 1
				};
				imageBlit.dstSubresource = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = i,
					.layerCount = 1,
				};
				imageBlit.dstOffsets[1] = {
					.x = int32_t(width >> i),
					.y = int32_t(height >> i),
					.z = 1
				};

				VkImageSubresourceRange mipSubRange
				{
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = i,
					.levelCount = 1,
					.layerCount = 1
				};
				{
					VkImageMemoryBarrier imageMemoryBarrier
					{
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
						.srcAccessMask = 0,
						.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
						.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
						.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						.image = image,
						.subresourceRange = mipSubRange
					};
					vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
				}
				vkCmdBlitImage(blitCmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);
				{
					VkImageMemoryBarrier imageMemoryBarrier
					{
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
						.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
						.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
						.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						.image = image,
						.subresourceRange = mipSubRange
					};
					vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
				}
			}

			//
			subresourceRange.levelCount = mipLevels;
			imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			{
				VkImageMemoryBarrier imageMemoryBarrier
				{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
					.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.image = image,
					.subresourceRange = subresourceRange
				};
				vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
			}

			if(deleteBuffer)
			{
				delete[] buffer;
			}
			device->flushCommandBuffer(blitCmd, copyQueue, true);
		}
		else
		{
			std::string filename = path + "/" + gltfImage.uri;

			ktxTexture* ktxTexture;
			ktxResult result = KTX_SUCCESS;

			if(!vkt::tools::fileExists(filename))
			{
				vkt::tools::exitFatal("could not load texture from " + filename + "\n\nMake sure assets submodule has been checked out and is up-to-date.", -1);
			}
			result = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
			assert(result == KTX_SUCCESS);

			this->device = device;
			width = ktxTexture->baseWidth;
			height = ktxTexture->baseHeight;
			mipLevels = ktxTexture->numLevels;

			ktx_uint8_t* ktxTextureData = ktxTexture_GetData(ktxTexture);
			ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTexture);
			format = ktxTexture_GetVkFormat(ktxTexture);


			VkFormatProperties formatProperties;
			vkGetPhysicalDeviceFormatProperties(device->physicalDevice, format, &formatProperties);
	
			VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
			VkBuffer stagingBuffer;
			VkDeviceMemory stagingMemory;

			VkBufferCreateInfo bufferCreateInfo
			{
				.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.size = ktxTextureSize,
				.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE
			};
			VK_CHECK_RESULT(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

			VkMemoryRequirements memReqs;
			vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);
			VkMemoryAllocateInfo memAllocInfo
			{
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.allocationSize = memReqs.size,
				.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
			};
			VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
			VK_CHECK_RESULT(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

			uint8_t* data{ nullptr };
			VK_CHECK_RESULT(vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));
			memcpy(data, ktxTextureData, ktxTextureSize);
			vkUnmapMemory(device->logicalDevice, stagingMemory);

			std::vector<VkBufferImageCopy> bufferCopyRegions;
			for (uint32_t i = 0; i < mipLevels; i++)
			{
				ktx_size_t offset;
				KTX_error_code result = ktxTexture_GetImageOffset(ktxTexture, i, 0, 0, &offset);
				assert(result == KTX_SUCCESS);
				VkBufferImageCopy bufferCopyRegion
				{
					.bufferOffset = offset,
					.imageSubresource = 
					{
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.mipLevel = i,
						.baseArrayLayer = 0,
						.layerCount = 1
					},
					.imageExtent = 
					{
						.width = std::max(1u, ktxTexture->baseWidth >> i),
						.height = std::max(1u, ktxTexture->baseHeight >> i),
						.depth = 1
					}
				};
				bufferCopyRegions.push_back(bufferCopyRegion);
			}

			// Create optimal tiled target image
			VkImageCreateInfo imageCreateInfo{
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = format,
				.extent = {.width = width, .height = height, .depth = 1 },
				.mipLevels = mipLevels,
				.arrayLayers = 1,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.tiling = VK_IMAGE_TILING_OPTIMAL,
				.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			};
			VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));

			vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs);
			memAllocInfo.allocationSize = memReqs.size;
			memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));
			VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));

			VkImageSubresourceRange subresourceRange
			{ 
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, 
				.baseMipLevel = 0, 
				.levelCount = mipLevels, 
				.layerCount = 1 
			};
			vkt::tools::setImageLayout(copyCmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
			vkCmdCopyBufferToImage(copyCmd, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(bufferCopyRegions.size()), bufferCopyRegions.data());
			vkt::tools::setImageLayout(copyCmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange);
			device->flushCommandBuffer(copyCmd, copyQueue);
			this->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);
			vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);

			ktxTexture_Destroy(ktxTexture);
		}

		VkSamplerCreateInfo samplerInfo
		{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter = VK_FILTER_LINEAR,
			.minFilter = VK_FILTER_LINEAR,
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
			.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
			.anisotropyEnable = VK_TRUE,
			.maxAnisotropy = 8.0f,
			.compareOp = VK_COMPARE_OP_NEVER,
			.maxLod = (float)mipLevels,
			.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
		};
		VK_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerInfo, nullptr, &sampler));

		VkImageViewCreateInfo viewCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = format,
			.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = mipLevels, .layerCount = 1}
		};
		VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCreateInfo, nullptr, &view));

		descriptor.sampler = sampler;
		descriptor.imageView = view;
		descriptor.imageLayout = imageLayout;
	}


	// gltfMaterial
	void Material::CreateDescriptorSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout, uint32_t descriptorBindingFlags)
	{
		VkDescriptorSetAllocateInfo descriptorSetAllocInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = descriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts = &descriptorSetLayout
		};
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device->logicalDevice, &descriptorSetAllocInfo, &descriptorSet));
		std::vector<VkDescriptorImageInfo> imageDescriptors{};
		std::vector<VkWriteDescriptorSet> writeDescriptorSets{};
		if (descriptorBindingFlags & DescriptorBindingFlags::ImageBaseColor)
		{
			imageDescriptors.push_back(baseColorTexture->descriptor);
			VkWriteDescriptorSet writeDescriptorSet
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSet,
				.dstBinding = static_cast<uint32_t>(writeDescriptorSets.size()),
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &baseColorTexture->descriptor
			};
			writeDescriptorSets.push_back(writeDescriptorSet);
		}
		if(normalTexture && descriptorBindingFlags & DescriptorBindingFlags::ImageNormalMap)
		{
			imageDescriptors.push_back(normalTexture->descriptor);
			VkWriteDescriptorSet writeDescriptorSet
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSet,
				.dstBinding = static_cast<uint32_t>(writeDescriptorSets.size()),
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &normalTexture->descriptor
			};
			writeDescriptorSets.push_back(writeDescriptorSet);
		}
		vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	// gltf primitive
	void Primitive::setDimensions(glm::vec3 min, glm::vec3 max)
	{
		dimensions.min = min;
		dimensions.max = max;
		dimensions.size = max - min;
		dimensions.center = (min + max) / 2.0f;
		dimensions.radius = glm::distance(min, max) / 2.0f;
	}

	// gltf mesh
	Mesh::Mesh(vkt::VulkanDevice* device, glm::mat4 matrix)
	{
		this->device = device;
		this->uniformBlock.matrix = matrix;
		VK_CHECK_RESULT(device->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(uniformBlock),
			&uniformBuffer.buffer,
			&uniformBuffer.memory,
			&uniformBlock)
		);
		VK_CHECK_RESULT(vkMapMemory(device->logicalDevice, uniformBuffer.memory, 0, sizeof(uniformBlock), 0, &uniformBuffer.mapped));
		uniformBuffer.descriptor =
		{
			uniformBuffer.buffer,
			0,
			sizeof(uniformBlock)
		};
	}

	Mesh::~Mesh()
	{
		vkDestroyBuffer(device->logicalDevice, uniformBuffer.buffer, nullptr);
		vkFreeMemory(device->logicalDevice, uniformBuffer.memory, nullptr);
		for(auto primitive : primitives)
		{
			delete primitive;
		}
	}

	// gltf node

	glm::mat4 Node::localMatrix()
	{
		return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation) * glm::scale(glm::mat4(1.0f), scale) * matrix;
	}

	glm::mat4 Node::getMatrix()
	{
		glm::mat4 m = localMatrix();
		vkglTF::Node* p = parent;
		while(p)
		{
			m = p->localMatrix() * m;
			p = p->parent;
		}
		return m;
	}

	void Node::update()
	{
		if(mesh)
		{
			glm::mat4 m = getMatrix();
			if(skin)
			{
				mesh->uniformBlock.matrix = m;
				// update joint matrices
				glm::mat4 inverseTransform = glm::inverse(m);
				for (size_t i = 0; i < skin->joints.size(); i++)
				{
					vkglTF::Node* jointNode = skin->joints[i];
					glm::mat4 jointMatrix = jointNode->getMatrix() * skin->inverseBindMatrices[i];
					jointMatrix = inverseTransform * jointMatrix;
					mesh->uniformBlock.jointMatrix[i] = jointMatrix;
				}
				mesh->uniformBlock.jointCount = (float)skin->joints.size();
				memcpy(mesh->uniformBuffer.mapped, &mesh->uniformBlock, sizeof(mesh->uniformBlock));
			}
			else
			{
				memcpy(mesh->uniformBuffer.mapped, &m, sizeof(glm::mat4));
			}
		}

		for(auto& child : children)
		{
			child->update();
		}
	}

	Node::~Node()
	{
		if(mesh)
		{
			delete mesh;
		}

		for(auto& child : children)
		{
			delete child;
		}
	}

	// gltf vertex layout to vulkan mapping


	VkVertexInputBindingDescription vkglTF::Vertex::vertexInputBindingDescription;
	std::vector<VkVertexInputAttributeDescription> vkglTF::Vertex::vertexInputAttributeDescriptions;
	VkPipelineVertexInputStateCreateInfo vkglTF::Vertex::pipelineVertexInputStateCreateInfo;

	
	VkVertexInputBindingDescription Vertex::inputBindingDescription(uint32_t binding)
	{
		return VkVertexInputBindingDescription({ binding, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX });
	}

	VkVertexInputAttributeDescription Vertex::inputAttributeDescription(uint32_t binding, uint32_t location, VertexComponent component)
	{
		switch(component)
		{
		case VertexComponent::Position:
			return VkVertexInputAttributeDescription({location, binding, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)});
		case VertexComponent::Normal:
			return VkVertexInputAttributeDescription({ location, binding, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal) });
		case VertexComponent::UV:
			return VkVertexInputAttributeDescription({ location, binding, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv) });
		case VertexComponent::Color:
			return VkVertexInputAttributeDescription({ location, binding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, color) });
		case VertexComponent::Tangent:
			return VkVertexInputAttributeDescription({ location, binding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, tangent) });
		case VertexComponent::Joint0:
			return VkVertexInputAttributeDescription({ location, binding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, joint0) });
		case VertexComponent::Weight0:
			return VkVertexInputAttributeDescription({ location, binding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, weight0) });
		default:
			return VkVertexInputAttributeDescription({});
		}

	}

	std::vector<VkVertexInputAttributeDescription> Vertex::inputAttributeDescriptions(uint32_t binding, const std::vector<VertexComponent> components)
	{
		std::vector<VkVertexInputAttributeDescription> result;
		uint32_t location = 0;
		for(VertexComponent component : components)
		{
			result.push_back(inputAttributeDescription(binding, location, component));
			location++;
		}
		return result;
	}

	VkPipelineVertexInputStateCreateInfo* Vertex::getPipelineVertexInputState(const std::vector<VertexComponent> components)
	{
		vertexInputBindingDescription = Vertex::inputBindingDescription(0);
		Vertex::vertexInputAttributeDescriptions = Vertex::inputAttributeDescriptions(0, components);
		pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
		pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = &Vertex::vertexInputBindingDescription;
		pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(Vertex::vertexInputAttributeDescriptions.size());
		pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = Vertex::vertexInputAttributeDescriptions.data();
		return &pipelineVertexInputStateCreateInfo;
	}


	// Model loading and rendering

	Model::~Model()
	{
		vkDestroyBuffer(device->logicalDevice, vertices.buffer, nullptr);
		vkFreeMemory(device->logicalDevice, vertices.memory, nullptr);
		vkDestroyBuffer(device->logicalDevice, indices.buffer, nullptr);
		vkFreeMemory(device->logicalDevice, indices.memory, nullptr);
		
		for(auto& texture : textures)
		{
			texture.destroy();
		}
		for(auto& skin : skins)
		{
			delete skin;
		}

		if(descriptorSetLayoutUbo != VK_NULL_HANDLE)
		{
			vkDestroyDescriptorSetLayout(device->logicalDevice, descriptorSetLayoutUbo, nullptr);
			descriptorSetLayoutUbo = VK_NULL_HANDLE;
		}
		if (descriptorSetLayoutImage != VK_NULL_HANDLE)
		{
			vkDestroyDescriptorSetLayout(device->logicalDevice, descriptorSetLayoutImage, nullptr);
			descriptorSetLayoutImage = VK_NULL_HANDLE;
		}
		
		vkDestroyDescriptorPool(device->logicalDevice, descriptorPool, nullptr);
		emptyTexture.destroy();
	}

	void Model::loadNode(vkglTF::Node* parent, const tinygltf::Node& node, uint32_t nodeIndex, const tinygltf::Model& model, std::vector<uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer, float globalScale)
	{
		vkglTF::Node* newNode= new Node{};
		newNode->index = nodeIndex;
		newNode->parent = parent;
		newNode->name = node.name;
		newNode->skinIndex = node.skin;
		newNode->matrix = glm::mat4(1.0f);

		glm::vec3 translation = glm::vec3(0.0f);
		if(node.translation.size() == 3)
		{
			translation = glm::make_vec3(node.translation.data());
			newNode->translation = translation;
		}
		glm::mat4 rotation = glm::mat4(1.0f);
		if(node.rotation.size() == 4)
		{
			glm::quat q = glm::make_quat(node.rotation.data());
			newNode->rotation = glm::mat4(q);
		}
		glm::vec3 scale = glm::vec3(1.0f);
		if(node.scale.size() == 3)
		{
			scale = glm::make_vec3(node.scale.data());
			newNode->scale = scale;
		}
		if(node.matrix.size() == 16)
		{
			newNode->matrix = glm::make_mat4x4(node.matrix.data());
		}

		if(node.children.size() > 0) 
		{
			for(auto i = 0; i < node.children.size(); i++)
			{
				loadNode(newNode, model.nodes[node.children[i]], node.children[i], model, indexBuffer, vertexBuffer, globalScale);
			}
		}

		// contains mesh data
		if(node.mesh > -1)
		{
			const tinygltf::Mesh mesh = model.meshes[node.mesh];
			Mesh* newMesh = new Mesh(device, newNode->matrix);
			newMesh->name = mesh.name;
			for(size_t j = 0; j < mesh.primitives.size(); j++)
			{
				const tinygltf::Primitive& primitive = mesh.primitives[j];
				if(primitive.indices < 0)
				{
					continue;
				}

				uint32_t indexStart = static_cast<uint32_t>(indexBuffer.size());
				uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.size());
				uint32_t indexCount = 0;
				uint32_t vertexCount = 0;
				glm::vec3 posMin{};
				glm::vec3 posMax{};
				bool hasSkin = false;
				
				// Vertices
				{
					const float* bufferPos = nullptr;
					const float* bufferNormals = nullptr;
					const float* bufferTexCoords = nullptr;
					const float* bufferColors = nullptr;
					const float* bufferTangents = nullptr;
					uint32_t numColorComponents;
					const uint16_t* bufferJoints = nullptr;
					const float* bufferWeights = nullptr;

					assert(primitive.attributes.find("POSITION") != primitive.attributes.end());

					const tinygltf::Accessor& posAcessor = model.accessors[primitive.attributes.find("POSITION")->second];
					const tinygltf::BufferView& posView = model.bufferViews[posAcessor.bufferView];
					bufferPos = reinterpret_cast<const float*>(&model.buffers[posView.buffer].data[posAcessor.byteOffset + posView.byteOffset]);
					posMin = glm::vec3(posAcessor.minValues[0], posAcessor.minValues[1], posAcessor.minValues[2]);
					posMax = glm::vec3(posAcessor.maxValues[0], posAcessor.maxValues[1], posAcessor.maxValues[2]);

					if(primitive.attributes.find("NORMAL") != primitive.attributes.end())
					{
						const tinygltf::Accessor& normAcessor = model.accessors[primitive.attributes.find("NORMAL")->second];
						const tinygltf::BufferView& normView = model.bufferViews[normAcessor.bufferView];
						bufferNormals = reinterpret_cast<const float*>(&model.buffers[normView.buffer].data[normAcessor.byteOffset + normView.byteOffset]);
					}

					if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end())
					{
						const tinygltf::Accessor& uvAcessor = model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
						const tinygltf::BufferView& uvView = model.bufferViews[uvAcessor.bufferView];
						bufferTexCoords = reinterpret_cast<const float*>(&model.buffers[uvView.buffer].data[uvAcessor.byteOffset + uvView.byteOffset]);
					}

					if (primitive.attributes.find("COLOR_0") != primitive.attributes.end())
					{
						const tinygltf::Accessor& colorAcessor = model.accessors[primitive.attributes.find("COLOR_0")->second];
						const tinygltf::BufferView& colorView = model.bufferViews[colorAcessor.bufferView];

						numColorComponents = colorAcessor.type == TINYGLTF_PARAMETER_TYPE_FLOAT_VEC3 ? 3 : 4;
						bufferColors = reinterpret_cast<const float*>(&model.buffers[colorView.buffer].data[colorAcessor.byteOffset + colorView.byteOffset]);
					}


					if (primitive.attributes.find("TANGENT") != primitive.attributes.end())
					{
						const tinygltf::Accessor& tangentAcessor = model.accessors[primitive.attributes.find("TANGENT")->second];
						const tinygltf::BufferView& tangentView = model.bufferViews[tangentAcessor.bufferView];
						bufferTangents = reinterpret_cast<const float*>(&model.buffers[tangentView.buffer].data[tangentAcessor.byteOffset + tangentView.byteOffset]);
					}

					// skinning and joints
					if (primitive.attributes.find("JOINTS_0") != primitive.attributes.end())
					{
						const tinygltf::Accessor& jointAcessor = model.accessors[primitive.attributes.find("JOINTS_0")->second];
						const tinygltf::BufferView& jointView = model.bufferViews[jointAcessor.bufferView];
						bufferJoints = reinterpret_cast<const uint16_t*>(&model.buffers[jointView.buffer].data[jointAcessor.byteOffset + jointView.byteOffset]);
					}

					if (primitive.attributes.find("WEIGHTS_0") != primitive.attributes.end())
					{
						const tinygltf::Accessor& uvAcessor = model.accessors[primitive.attributes.find("WEIGHTS_0")->second];
						const tinygltf::BufferView& uvView = model.bufferViews[uvAcessor.bufferView];
						bufferWeights = reinterpret_cast<const float*>(&model.buffers[uvView.buffer].data[uvAcessor.byteOffset + uvView.byteOffset]);
					}

					hasSkin = (bufferJoints && bufferWeights);
					vertexCount = static_cast<uint32_t>(posAcessor.count);
					
					for(size_t v = 0; v < posAcessor.count; v++)
					{
						Vertex vertex{};
						vertex.pos = glm::vec4(glm::make_vec3(&bufferPos[v * 3]), 1.0f);
						vertex.normal = glm::normalize(glm::vec3(bufferNormals ? glm::make_vec3(&bufferNormals[v * 3]) : glm::vec3(0.0f)));
						vertex.uv = bufferTexCoords ? glm::make_vec2(&bufferTexCoords[v * 2]) : glm::vec3(0.0f);
						if(bufferColors)
						{
							switch(numColorComponents)
							{
							case 3:
								vertex.color = glm::vec4(glm::make_vec3(&bufferColors[v * 3]), 1.0f);
								break;
								vertex.color = glm::make_vec4(&bufferColors[v * 4]);
								break;
							}
						}
						else
						{
							vertex.color = glm::vec4(1.0f);
						}
						vertex.tangent = bufferTangents ? glm::vec4(glm::make_vec4(&bufferTangents[v * 4])) : glm::vec4(0.0f);
						vertex.joint0 = hasSkin ? glm::vec4(glm::make_vec4(&bufferJoints[v * 4])) : glm::vec4(0.0f);
						vertex.weight0 = hasSkin ? glm::vec4(glm::make_vec4(&bufferWeights[v * 4])) : glm::vec4(0.0f);
						vertexBuffer.push_back(vertex);
					}
				}
				// indices
				{
					const tinygltf::Accessor& accessor = model.accessors[primitive.indices];
					const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
					const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

					indexCount = static_cast<uint32_t>(accessor.count);

					switch (accessor.componentType) 
					{
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
					{
						uint32_t* buf = new uint32_t[accessor.count];
						memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint32_t));
						for (size_t index = 0; index < accessor.count; index++) {
							indexBuffer.push_back(buf[index] + vertexStart);
						}
						delete[] buf;
						break;
					}
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
					{
						uint16_t* buf = new uint16_t[accessor.count];
						memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint16_t));
						for (size_t index = 0; index < accessor.count; index++) {
							indexBuffer.push_back(buf[index] + vertexStart);
						}
						delete[] buf;
						break;
					}
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
					{
						uint8_t* buf = new uint8_t[accessor.count];
						memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint8_t));
						for (size_t index = 0; index < accessor.count; index++) {
							indexBuffer.push_back(buf[index] + vertexStart);
						}
						delete[] buf;
						break;
					}
					default:
						std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
						return;
					}
				}
				Primitive* newPrimitive = new Primitive(indexStart, indexCount, primitive.material > -1 ? materials[primitive.material] : materials.back());
				newPrimitive->firstVertex = vertexStart;
				newPrimitive->vertexCount = vertexCount;
				newPrimitive->setDimensions(posMin, posMax);
				newMesh->primitives.push_back(newPrimitive);
			}
			newNode->mesh = newMesh;
		}
		if(parent)
		{
			parent->children.push_back(newNode);
		}
		else
		{
			nodes.push_back(newNode);
		}
		linearNodes.push_back(newNode);
	}

	void Model::loadSkins(tinygltf::Model& gltfmodel)
	{
		for(tinygltf::Skin& source : gltfmodel.skins)
		{
			Skin* newSkin = new Skin{};
			newSkin->name = source.name;

			if(source.skeleton > - 1)
			{
				newSkin->skeletonRoot = nodeFromIndex(source.skeleton);
			}

			for (int jointIndex : source.joints) 
			{
				Node* node = nodeFromIndex(jointIndex);
				if(node)
				{
					newSkin->joints.push_back(nodeFromIndex(jointIndex));
				}
			}

			if(source.inverseBindMatrices > -1)
			{
				const tinygltf::Accessor& acessor = gltfmodel.accessors[source.inverseBindMatrices];
				const tinygltf::BufferView& bufferView = gltfmodel.bufferViews[acessor.bufferView];
				const tinygltf::Buffer& buffer = gltfmodel.buffers[bufferView.buffer];
				newSkin->inverseBindMatrices.resize(acessor.count);
				memcpy(newSkin->inverseBindMatrices.data(), &buffer.data[acessor.byteOffset + bufferView.byteOffset], acessor.count * sizeof(glm::mat4));
			}

			skins.push_back(newSkin);
		}
	}

	void Model::loadImages(tinygltf::Model& gltfmodel, vkt::VulkanDevice* device, VkQueue transferQueue)
	{
		for (tinygltf::Image& image : gltfmodel.images)
		{
			vkglTF::Texture texture;
			texture.fromgltfImage(image, path, device, transferQueue);
			texture.index = static_cast<uint32_t>(textures.size());
			textures.push_back(texture);
		}

		// create emptyt exture to be used for empty material images
		createEmptyTexture(transferQueue);
	}

	void Model::loadMaterials(tinygltf::Model& gltfmodel)
	{
		for(tinygltf::Material& mat : gltfmodel.materials)
		{
			vkglTF::Material material(device);
			if (mat.values.find("baseColorTexture") != mat.values.end()) 
			{
				material.baseColorTexture = getTexture(gltfmodel.textures[mat.values["baseColorTexture"].TextureIndex()].source);
			}
			// Metallic roughness workflow
			if (mat.values.find("metallicRoughnessTexture") != mat.values.end()) 
			{
				material.metallicRoughnessTexture = getTexture(gltfmodel.textures[mat.values["metallicRoughnessTexture"].TextureIndex()].source);
			}
			if (mat.values.find("roughnessFactor") != mat.values.end()) 
			{
				material.roughnessFactor = static_cast<float>(mat.values["roughnessFactor"].Factor());
			}
			if (mat.values.find("metallicFactor") != mat.values.end()) 
			{
				material.metallicFactor = static_cast<float>(mat.values["metallicFactor"].Factor());
			}
			if (mat.values.find("baseColorFactor") != mat.values.end())
			{
				material.baseColorFactor = glm::make_vec4(mat.values["baseColorFactor"].ColorFactor().data());
			}
			if (mat.additionalValues.find("normalTexture") != mat.additionalValues.end()) 
			{
				material.normalTexture = getTexture(gltfmodel.textures[mat.additionalValues["normalTexture"].TextureIndex()].source);
			}
			else {
				material.normalTexture = &emptyTexture;
			}
			if (mat.additionalValues.find("emissiveTexture") != mat.additionalValues.end()) 
			{
				material.emissiveTexture = getTexture(gltfmodel.textures[mat.additionalValues["emissiveTexture"].TextureIndex()].source);
			}
			if (mat.additionalValues.find("occlusionTexture") != mat.additionalValues.end()) 
			{
				material.occlusionTexture = getTexture(gltfmodel.textures[mat.additionalValues["occlusionTexture"].TextureIndex()].source);
			}
			if (mat.additionalValues.find("alphaMode") != mat.additionalValues.end()) 
			{
				tinygltf::Parameter param = mat.additionalValues["alphaMode"];
				if (param.string_value == "BLEND") {
					material.alphaMode = Material::ALPHAMODE_BLEND;
				}
				if (param.string_value == "MASK") {
					material.alphaMode = Material::ALPHAMODE_MASK;
				}
			}
			if (mat.additionalValues.find("alphaCutoff") != mat.additionalValues.end()) 
			{
				material.alphaCutoff = static_cast<float>(mat.additionalValues["alphaCutoff"].Factor());
			}

			materials.push_back(material);
		}

		// default material at the end of the lsit for meshes with no material assigned
		materials.push_back(Material(device));
	}

	void Model::loadAnimations(tinygltf::Model& gltfmodel)
	{
		for(tinygltf::Animation& anim : gltfmodel.animations)
		{
			vkglTF::Animation animation{};
			animation.name = anim.name;
			if(anim.name.empty())
			{
				animation.name = std::to_string(animations.size());
			}

			// samplers
			for(auto& samp : anim.samplers)
			{
				vkglTF::AnimationSampler sampler{};
				if(samp.interpolation == "LINEAR")
				{
					sampler.interpolation = AnimationSampler::InterpolationType::LINEAR;
				}
				if (samp.interpolation == "STEP")
				{
					sampler.interpolation = AnimationSampler::InterpolationType::STEP;
				}
				if (samp.interpolation == "CUBICSPLINE")
				{
					sampler.interpolation = AnimationSampler::InterpolationType::CUBICSPLINE;
				}

				{
					const tinygltf::Accessor& acessor = gltfmodel.accessors[samp.input];
					const tinygltf::BufferView& bufferView = gltfmodel.bufferViews[acessor.bufferView];
					const tinygltf::Buffer& buffer = gltfmodel.buffers[bufferView.buffer];

					assert(acessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

					float* buf = new float[acessor.count];
					memcpy(buf, &buffer.data[acessor.byteOffset + bufferView.byteOffset], acessor.count * sizeof(float));
					for(size_t index = 0; index < acessor.count; index++)
					{
						sampler.inputs.push_back(buf[index]);
					}
					delete[] buf;
					for(auto input : sampler.inputs)
					{
						if(input < animation.start)
						{
							animation.start = input;
						}
						if(input > animation.end)
						{
							animation.end = input;
						}
					}
				}

				{
					const tinygltf::Accessor& acessor = gltfmodel.accessors[samp.output];
					const tinygltf::BufferView& bufferView = gltfmodel.bufferViews[acessor.bufferView];
					const tinygltf::Buffer& buffer = gltfmodel.buffers[bufferView.buffer];

					assert(acessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

					switch(acessor.type)
					{
						case TINYGLTF_TYPE_VEC3:
						{
							glm::vec3* buf = new glm::vec3[acessor.count];
							memcpy(buf, &buffer.data[acessor.byteOffset + bufferView.byteOffset], acessor.count * sizeof(glm::vec3));
							for(size_t index = 0; index < acessor.count; index++)
							{
								sampler.outputsVec4.push_back(glm::vec4(buf[index], 0.0f));
							}
							delete[] buf;
							break;
						}
						case TINYGLTF_TYPE_VEC4:
						{
							glm::vec4* buf = new glm::vec4[acessor.count];
							memcpy(buf, &buffer.data[acessor.byteOffset + bufferView.byteOffset], acessor.count * sizeof(glm::vec4));
							for (size_t index = 0; index < acessor.count; index++)
							{
								sampler.outputsVec4.push_back(buf[index]);
							}
							delete[] buf;
							break;
						}
						default:
						{
							std::cout << "unknown type" << std::endl;
							break;
						}
					}
				}
				animation.samplers.push_back(sampler);
			}

			for(auto& source : anim.channels)
			{
				vkglTF::AnimationChannel channel{};
				if(source.target_path == "rotation")
				{
					channel.path = AnimationChannel::PathType::ROTATION;
				}
				if (source.target_path == "translation")
				{
					channel.path = AnimationChannel::PathType::TRANSLATION;
				}
				if (source.target_path == "scale")
				{
					channel.path = AnimationChannel::PathType::SCALE;
				}
				if(source.target_path == "weights")
				{
					std::cout << "weights not yet supported, skipping channel" << std::endl;
					continue;
				}
				channel.samplerIndex = source.sampler;
				channel.node = nodeFromIndex(source.target_node);
				if(!channel.node)
				{
					continue;
				}
				animation.channels.push_back(channel);
			}

			animations.push_back(animation);
		}
	}

	void Model::loadFromFile(std::string filename, vkt::VulkanDevice* device, VkQueue transferQueue, uint32_t fileLoadingFlags, float scale)
	{
		tinygltf::Model gltfModel;
		tinygltf::TinyGLTF gltfContext;
		if(fileLoadingFlags & FileLoadingFlags::DontLoadImages)
		{
			gltfContext.SetImageLoader(loadImageDataFuncEmpty, nullptr);
		}
		else
		{
			gltfContext.SetImageLoader(loadImageDataFunc, nullptr);
		}

		size_t pos = filename.find_last_of('/');
		path = filename.substr(0, pos);

		std::string error;
		std::string warning;

		this->device = device;

		bool fileLoaded = gltfContext.LoadASCIIFromFile(&gltfModel, &error, &warning, filename);
		
		std::vector<uint32_t> indexBuffer;
		std::vector<Vertex> vertexBuffer;

		if(fileLoaded)
		{
			if(!fileLoadingFlags & FileLoadingFlags::DontLoadImages)
			{
				loadImages(gltfModel, device, transferQueue);
			}
			loadMaterials(gltfModel);
			const tinygltf::Scene& scene = gltfModel.scenes[gltfModel.defaultScene > -1 ? gltfModel.defaultScene : 0];
			for(size_t i = 0; i < scene.nodes.size(); i++)
			{
				const tinygltf::Node node = gltfModel.nodes[scene.nodes[i]];
				loadNode(nullptr, node, scene.nodes[i], gltfModel, indexBuffer, vertexBuffer, scale);
			}
			if(gltfModel.animations.size() > 0)
			{
				loadAnimations(gltfModel);
			}
			loadSkins(gltfModel);

			for(auto node : linearNodes)
			{
				if(node->skinIndex > -1)
				{
					node->skin = skins[node->skinIndex];
				}

				if(node->mesh)
				{
					node->update();
				}
			}
		}
		else
		{
			vkt::tools::exitFatal("Could not load glTF file \"" + filename + "\:" + error, -1);
			return;
		}

		if((fileLoadingFlags & FileLoadingFlags::PreTransformVertices) || (fileLoadingFlags & FileLoadingFlags::PreMultiplyVertexColors) || (fileLoadingFlags & FileLoadingFlags::FlipY))
		{
			const bool preTrasnform = fileLoadingFlags & FileLoadingFlags::PreTransformVertices;
			const bool preMultiplyColor = fileLoadingFlags & FileLoadingFlags::PreMultiplyVertexColors;
			const bool flipY = fileLoadingFlags & FileLoadingFlags::FlipY;
			const bool flipUV = fileLoadingFlags & FileLoadingFlags::FlipUV;

			for(Node* node : linearNodes)
			{
				if(node->mesh)
				{
					const glm::mat4 localMatrix = node->getMatrix();
					for(Primitive* primitive : node->mesh->primitives)
					{
						for(uint32_t i = 0; i < primitive->vertexCount; i++)
						{
							Vertex& vertex = vertexBuffer[primitive->firstVertex + i];

							if(preTrasnform)
							{
								vertex.pos = glm::vec3(localMatrix * glm::vec4(vertex.pos, 1.0f));
								vertex.normal = glm::normalize(glm::mat3(localMatrix) * vertex.normal);
							}

							if(flipY)
							{
								vertex.pos.y *= -1.0f;
								vertex.normal.y *= -1.0f;
							}

							if(flipUV)
							{
								vertex.uv.t = 1.0f - vertex.uv.t;
							}

							if(preMultiplyColor)
							{
								vertex.color = primitive->material.baseColorFactor * vertex.color;
							}
						}
					}
				}
			}
		}

		for(auto& extension : gltfModel.extensionsUsed)
		{
			if(extension == "KHR_materials_pbrSpecularGlossiness")
			{
				std::cout << "Required extension " << extension;
				metallicRoughnessWorkflow = false;
			}
		}

		size_t vertexBufferSize = vertexBuffer.size() * sizeof(Vertex);
		size_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);

		assert((vertexBufferSize > 0) && (indexBufferSize > 0));

		struct StagingBuffer
		{
			VkBuffer buffer;
			VkDeviceMemory memory;
		} vertexStaging{}, indexStaging{};

		// create staging buffers
		VK_CHECK_RESULT(device->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			vertexBufferSize,
			&vertexStaging.buffer,
			&vertexStaging.memory,
			vertexBuffer.data()
		));

		VK_CHECK_RESULT(device->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			indexBufferSize,
			&indexStaging.buffer,
			&indexStaging.memory,
			indexBuffer.data()
		));

		// create device local buffers
		VK_CHECK_RESULT(device->createBuffer(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | memoryPropertyFlags,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			vertexBufferSize,
			&vertices.buffer,
			&vertices.memory
		));

		VK_CHECK_RESULT(device->createBuffer(
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | memoryPropertyFlags,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			indexBufferSize,
			&indices.buffer,
			&indices.memory
		));

		VkCommandBuffer copyCommand = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy copyRegion = {};

		copyRegion.size = vertexBufferSize;
		vkCmdCopyBuffer(copyCommand, vertexStaging.buffer, vertices.buffer, 1, &copyRegion);

		copyRegion.size = indexBufferSize;
		vkCmdCopyBuffer(copyCommand, indexStaging.buffer, indices.buffer, 1, &copyRegion);

		device->flushCommandBuffer(copyCommand, transferQueue, true);

		vkDestroyBuffer(device->logicalDevice, vertexStaging.buffer, nullptr);
		vkFreeMemory(device->logicalDevice, vertexStaging.memory, nullptr);
		vkDestroyBuffer(device->logicalDevice, indexStaging.buffer, nullptr);
		vkFreeMemory(device->logicalDevice, indexStaging.memory, nullptr);

		getSceneDimensions();

		// setup descriptors
		uint32_t uboCount{ 0 };
		uint32_t imageCount{ 0 };
		for(auto& node : linearNodes)
		{
			if(node->mesh)
			{
				uboCount++;
			}
		}

		for(auto& material : materials)
		{
			if(material.baseColorTexture != nullptr)
			{
				imageCount++;
			}
		}

		std::vector<VkDescriptorPoolSize> poolSizes =
		{
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uboCount }
		};

		if(imageCount > 0)
		{
			if(descriptorBindingFlags & DescriptorBindingFlags::ImageBaseColor)
			{
				poolSizes.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageCount });
			}

			if (descriptorBindingFlags & DescriptorBindingFlags::ImageNormalMap)
			{
				poolSizes.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageCount });
			}
		}
		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.maxSets = uboCount + imageCount,
			.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
			.pPoolSizes = poolSizes.data()
		};
		VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolCreateInfo, nullptr, &descriptorPool));

		// descriptors for per node uniform buffers
		{
			if(descriptorSetLayoutUbo == VK_NULL_HANDLE)
			{
				VkDescriptorSetLayoutBinding setLayoutBinding =
				{
					.binding = 0,
					.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.descriptorCount = 1,
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
				};
				VkDescriptorSetLayoutCreateInfo descriptorLayoutCreateInfo
				{
					.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
					.bindingCount = 1,
					.pBindings = &setLayoutBinding
				};
				VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorLayoutCreateInfo, nullptr, &descriptorSetLayoutUbo));
			}
			for(auto node : nodes)
			{
				prepareNodeDescriptor(node, descriptorSetLayoutUbo);
			}
		}

		// descriptors for per material images
		{
			if(descriptorSetLayoutImage == VK_NULL_HANDLE)
			{
				std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
				if(descriptorBindingFlags & DescriptorBindingFlags::ImageBaseColor)
				{
					setLayoutBindings.push_back(
						{
							.binding = static_cast<uint32_t>(setLayoutBindings.size()),
							.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
							.descriptorCount = 1,
							.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
						}
					);
				}
				if (descriptorBindingFlags & DescriptorBindingFlags::ImageNormalMap)
				{
					setLayoutBindings.push_back(
						{
							.binding = static_cast<uint32_t>(setLayoutBindings.size()),
							.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
							.descriptorCount = 1,
							.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
						}
					);
				}
				VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo
				{
					.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
					.bindingCount = static_cast<uint32_t>(setLayoutBindings.size()),
					.pBindings = setLayoutBindings.data()
				};
				VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayoutImage));
			}
			for(auto& material : materials)
			{
				if(material.baseColorTexture != nullptr)
				{
					material.CreateDescriptorSet(descriptorPool, descriptorSetLayoutImage, descriptorBindingFlags);
				}
			}
		}

	}

	void Model::bindBuffers(VkCommandBuffer commandBuffer)
	{
		const VkDeviceSize offsets[1] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertices.buffer, offsets);
		vkCmdBindIndexBuffer(commandBuffer, indices.buffer, 0, VK_INDEX_TYPE_UINT32);
		buffersBound = true;
	}

	void Model::drawNode(Node* node, VkCommandBuffer commandBuffer, uint32_t renderFlags, VkPipelineLayout pipelineLayout, uint32_t bindImageSet)
	{
		if(node->mesh)
		{
			for(Primitive* primitive : node->mesh->primitives)
			{
				bool skip = false;
				const vkglTF::Material& material = primitive->material;
				if(renderFlags & RenderFlags::RenderOpaqueNodes)
				{
					skip = (material.alphaMode != Material::ALPHAMODE_OPAQUE);
				}
				if (renderFlags & RenderFlags::RenderAlphaMaskedNodes)
				{
					skip = (material.alphaMode != Material::ALPHAMODE_MASK);
				}
				if (renderFlags & RenderFlags::RenderAlphaBlendedNodes)
				{
					skip = (material.alphaMode != Material::ALPHAMODE_BLEND);
				}
				if(!skip)
				{
					if(renderFlags & RenderFlags::BindImages)
					{
						vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, bindImageSet, 1, &material.descriptorSet, 0, nullptr);
					}
					vkCmdDrawIndexed(commandBuffer, primitive->indexCount, 1, primitive->firstIndex, 0, 0);
				}
			}
		}
		for(auto& child : node->children)
		{
			drawNode(child, commandBuffer, renderFlags, pipelineLayout, bindImageSet);
		}
	}

	void Model::draw(VkCommandBuffer commandBuffer, uint32_t renderFlags, VkPipelineLayout pipelineLayout, uint32_t bindImageSet)
	{
		if(!buffersBound)
		{
			const VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertices.buffer, offsets);
			vkCmdBindIndexBuffer(commandBuffer, indices.buffer, 0, VK_INDEX_TYPE_UINT32);
		}
		for(auto& node : nodes)
		{
			drawNode(node, commandBuffer, renderFlags, pipelineLayout, bindImageSet);
		}
	}

	void Model::getNodeDimensions(Node* node, glm::vec3& min, glm::vec3& max)
	{
		if(node->mesh)
		{
			for(Primitive* primitive : node->mesh->primitives)
			{
				glm::vec4 locMin = glm::vec4(primitive->dimensions.min, 1.0f) * node->getMatrix();
				glm::vec4 locMax = glm::vec4(primitive->dimensions.max, 1.0f) * node->getMatrix();
				if (locMin.x < min.x) { min.x = locMin.x; }
				if (locMin.y < min.y) { min.y = locMin.y; }
				if (locMin.z < min.z) { min.z = locMin.z; }
				if (locMax.x > max.x) { max.x = locMax.x; }
				if (locMax.y > max.y) { max.y = locMax.y; }
				if (locMax.z > max.z) { max.z = locMax.z; }
			}
		}
		for(auto& child : node->children)
		{
			getNodeDimensions(child, min, max);
		}
	}

	void Model::getSceneDimensions()
	{
		dimensions.min = glm::vec3(FLT_MAX);
		dimensions.max = glm::vec3(-FLT_MAX);

		for(auto node : nodes)
		{
			getNodeDimensions(node, dimensions.min, dimensions.max);
		}
		dimensions.size = dimensions.max - dimensions.min;
		dimensions.center = (dimensions.min + dimensions.max) / 2.0f;
		dimensions.radius = glm::distance(dimensions.min, dimensions.max) / 2.0f;
	}

	void Model::updateAnimation(uint32_t index, float time)
	{
		if(index > static_cast<uint32_t>(animations.size()) - 1)
		{
			std::cout << "No animation with index " << index << std::endl;
			return;
		}
		Animation& animation = animations[index];
		
		bool updated = false;

		for(auto& channel : animation.channels)
		{
			vkglTF::AnimationSampler& sampler = animation.samplers[channel.samplerIndex];
			if(sampler.inputs.size() > sampler.outputsVec4.size())
			{
				continue;
			}

			for(auto i = 0; i < sampler.inputs.size() - 1; i++)
			{
				if((time >= sampler.inputs[i]) && (time <= sampler.inputs[i + 1]))
				{
					float u = std::max(0.0f, time - sampler.inputs[i]) / (sampler.inputs[i + 1] - sampler.inputs[i]);
					if(u <= 1.0f)
					{
						switch (channel.path)
						{
							case vkglTF::AnimationChannel::TRANSLATION:
							{
								glm::vec4 trans = glm::mix(sampler.outputsVec4[i], sampler.outputsVec4[i + 1], u);
								channel.node->translation = glm::vec3(trans);
								break;
							}
							case vkglTF::AnimationChannel::SCALE:
							{
								glm::vec4 scale = glm::mix(sampler.outputsVec4[i], sampler.outputsVec4[i + 1], u);
								channel.node->scale = glm::vec3(scale);
								break;
							}
							case vkglTF::AnimationChannel::ROTATION:
							{
								glm::quat q1;
								q1.x = sampler.outputsVec4[i].x;
								q1.y = sampler.outputsVec4[i].y;
								q1.z = sampler.outputsVec4[i].z;
								q1.w = sampler.outputsVec4[i].w;
								glm::quat q2;
								q2.x = sampler.outputsVec4[i].x;
								q2.y = sampler.outputsVec4[i].y;
								q2.z = sampler.outputsVec4[i].z;
								q2.w = sampler.outputsVec4[i].w;
								channel.node->rotation = glm::normalize(glm::slerp(q1, q2, u));
								break;
							}
						}
						updated = true;
					}
				}
			}
		}
		if(updated)
		{
			for(auto& node : nodes)
			{
				node->update();
			}
		}
	}
	/// Helper Functions

	Node* Model::findNode(Node* parent, uint32_t index)
	{
		Node* nodeFound = nullptr;
		if(parent->index == index)
		{
			return parent;
		}
		for(auto& child : parent->children)
		{
			findNode(child, index);
			if(nodeFound)
			{
				break;
			}
		}

		return nodeFound;
	}

	Node* Model::nodeFromIndex(uint32_t index)
	{
		Node* nodeFound = nullptr;
		for(auto& node : nodes)
		{
			nodeFound = findNode(node, index);
			if(nodeFound)
			{
				break;
			}
		}
		return nodeFound;
	}

	Node* Model::nodeFromName(const std::string name)
	{
		Node* nodeFound = nullptr;
		for(auto& node : linearNodes)
		{
			if(node->name == name)
			{
				return node;
			}
		}
		return nullptr;
	}

	void Model::prepareNodeDescriptor(vkglTF::Node* node, VkDescriptorSetLayout descriptorSetLayout)
	{
		if(node->mesh)
		{
			VkDescriptorSetAllocateInfo descriptorSetAllocInfo
			{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = descriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &descriptorSetLayout
			};
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device->logicalDevice, &descriptorSetAllocInfo, &node->mesh->uniformBuffer.descriptorSet));
			VkWriteDescriptorSet writeDescriptorSet
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = node->mesh->uniformBuffer.descriptorSet,
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.pBufferInfo = &node->mesh->uniformBuffer.descriptor
			};
			vkUpdateDescriptorSets(device->logicalDevice, 1, &writeDescriptorSet, 0, nullptr);
		}
		for(auto child : node->children)
		{
			prepareNodeDescriptor(child, descriptorSetLayout);
		}
	}

	/// 

	vkglTF::Texture* Model::getTexture(uint32_t index)
	{
		if(index < textures.size())
		{
			return &textures[index];
		}
		return nullptr;

	}
	
	void Model::createEmptyTexture(VkQueue transferQueue)
	{
		emptyTexture.device = device;
		emptyTexture.width = 1;
		emptyTexture.height = 1;
		emptyTexture.layerCount = 1;
		emptyTexture.mipLevels = 1;

		size_t bufferSize = emptyTexture.width * emptyTexture.height * 4;
		unsigned char* buffer = new unsigned char[bufferSize];
		memset(buffer, 0, bufferSize);

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;
		VkBufferCreateInfo bufferCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = bufferSize,
			.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
		};
		VK_CHECK_RESULT(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

		VkMemoryRequirements memoryReqs;
		vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memoryReqs);
		VkMemoryAllocateInfo memoryAllocateInfo
		{
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = memoryReqs.size,
			.memoryTypeIndex = device->getMemoryType(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
		};
		VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memoryAllocateInfo, nullptr, &stagingMemory));
		VK_CHECK_RESULT(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

		uint8_t* data{ nullptr };
		VK_CHECK_RESULT(vkMapMemory(device->logicalDevice, stagingMemory, 0, memoryReqs.size, 0, (void**)data));

		vkGetImageMemoryRequirements(device->logicalDevice, emptyTexture.image, &memoryReqs);
		memoryAllocateInfo.allocationSize = memoryReqs.size;
		memoryAllocateInfo.memoryTypeIndex = device->getMemoryType(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memoryAllocateInfo, nullptr, &emptyTexture.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, emptyTexture.image, emptyTexture.deviceMemory, 0));

		VkBufferImageCopy bufferCopyRegion
		{
			.imageSubresource =
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.layerCount = 1
			},
			.imageExtent =
			{
				.width = emptyTexture.width,
				.height = emptyTexture.height,
				.depth = 1
			}
		};

		VkImageSubresourceRange subResourceRange
		{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.layerCount = 1
		};

		VkCommandBuffer copyCommand = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		vkt::tools::setImageLayout(copyCommand, emptyTexture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subResourceRange);
		vkCmdCopyBufferToImage(copyCommand, stagingBuffer, emptyTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);
		vkt::tools::setImageLayout(copyCommand, emptyTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subResourceRange);
		device->flushCommandBuffer(copyCommand, transferQueue);
		emptyTexture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);
		vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);

		VkSamplerCreateInfo samplerCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter = VK_FILTER_LINEAR,
			.minFilter = VK_FILTER_LINEAR,
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.maxAnisotropy = 1.0F,
			.compareOp = VK_COMPARE_OP_NEVER
		};
		VK_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerCreateInfo, nullptr, &emptyTexture.sampler));
		
		VkImageViewCreateInfo imageViewCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = emptyTexture.image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = VK_FORMAT_R8G8B8A8_UNORM,
			.subresourceRange =
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};
		VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &imageViewCreateInfo, nullptr, &emptyTexture.view));

		emptyTexture.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		emptyTexture.descriptor.imageView = emptyTexture.view;
		emptyTexture.descriptor.sampler = emptyTexture.sampler;

		delete[] buffer;
	}
}
