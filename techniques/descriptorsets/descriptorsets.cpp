#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

class VulkanExample : public VulkanExampleBase
{
public:
	VulkanExample() : VulkanExampleBase()
	{
		title = "using descriptor types";
		camera.type = Camera::CameraType::lookat;
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 512.0f);
		camera.setRotation(glm::vec3(0.0f));
		camera.setTranslation(glm::vec3(0.0f, 0.0f, -5.0f));

		apiVersion = VK_API_VERSION_1_3;
		enabledVulkan13Features.dynamicRendering = VK_TRUE;
		enabledVulkan13Features.synchronization2 = VK_TRUE;
		deviceCreatepNextChain = &enabledVulkan13Features;
	}

	~VulkanExample()
	{
		if(device)
		{
			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

			vkDestroyCommandPool(device, commandPool, nullptr);

			for (size_t i = 0; i < presentCompleteSemaphores.size(); i++)
			{
				vkDestroySemaphore(device, presentCompleteSemaphore[i], nullptr);
			}

			for (size_t i = 0; i < renderCompleteSemaphores.size(); i++)
			{
				vkDestroySemaphore(device, renderCompleteSemaphores[i], nullptr);
			}

			for(auto& cube : cubes)
			{
				for(auto& buffer : cube.uniformBuffers)
				{
					vkDestroyBuffer(device, buffer.handle, nullptr);
					vkFreeMemory(device, buffer.memory, nullptr);
				}
			}

			for(uint32_t i = 0; i < maxConcurrentFrames; i++)
			{
				vkDestroyFence(device, waitFences[i], nullptr);
			}
		}
	}

	virtual void getEnabledFeatures()
	{
		if(deviceProperties.apiVersion < VK_API_VERSION_1_3)
		{
			vkt::tools::exitFatal("selected gpou does not support vulkan 1.3", VK_ERROR_INCOMPATIBLE_DRIVER);
		}

		if(deviceFeatures.samplerAnisotropy)
		{
			enabledFeatures.samplerAnisotropy;
		}
	}

	uint32_t getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties)
	{
		for (uint32_t i = 0; i < deviceMemoryProperties.memoryTypeCount; i++)
		{
			if ((typeBits & 1) == 1)
			{
				if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
				{
					return i;
				}
			}
			typeBits >>= 1;
		}
		throw "could not find a suitable memory type";
	}

	VkShaderModule loadSPIRVShader(const std::string& filename)
	{
		size_t shaderSize;
		char* shaderCode{ nullptr };

		std::ifstream is(filename, std::ios::binary | std::ios::in | std::ios::ate);

		if (is.is_open())
		{
			shaderSize = is.tellg();
			is.seekg(0, std::ios::beg);

			shaderCode = new char[shaderSize];
			is.read(shaderCode, shaderSize);
			is.close();
			assert(shaderSize > 0);
		}

		if (shaderCode)
		{
			VkShaderModuleCreateInfo shaderModuleCreateInfo
			{
				.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
				.codeSize = shaderSize,
				.pCode = (uint32_t*)shaderCode
			};
			VkShaderModule shaderModule;
			VK_CHECK_RESULT(vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule));
			delete[] shaderCode;
			return shaderModule;
		}
		else
		{
			std::cerr << "Error: could not open shader file \"" << filename << "\"" << std::endl;
		}
	}

	void setupDepthStencil() override
	{
		VkImageCreateInfo imageCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = depthFormat,
			.extent = { width, height, 1 },
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.tiling = VK_IMAGE_TILING_OPTIMAL,
			.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
		};
		VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &depthStencil.image));

		VkMemoryRequirements memoryReqs;
		vkGetImageMemoryRequirements(device, depthStencil.image, &memoryReqs);
		VkMemoryAllocateInfo memoryAllocateInfo
		{
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = memoryReqs.size,
			.memoryTypeIndex = getMemoryTypeIndex(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		};
		VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &depthStencil.memory));
		VK_CHECK_RESULT(vkBindImageMemory(device, depthStencil.image, depthStencil.memory, 0));

		VkImageViewCreateInfo depthStencilViewCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = depthStencil.image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = depthFormat,
			.subresourceRange =
			{
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};

		if (depthFormat >= VK_FORMAT_D16_UNORM_S8_UINT)
		{
			depthStencilViewCreateInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		VK_CHECK_RESULT(vkCreateImageView(device, &depthStencilViewCreateInfo, nullptr, &depthStencil.view));
	}

	void loadAssets()
	{
		const uint32_t gltfLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
		model.loadFromFile(getAssetPath() + "models/cube.gltf", vulkanDevice, queue, gltfLoadingFlags);
		cubes[0].texture.loadFromFile(getAssetPath() + "textures/crate01_color_height_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
		cubes[1].texture.loadFromFile(getAssetPath() + "textures/crate02_color_height_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
	}

	void setupDescriptors()
	{
		// where we allocate the descriptors from
		std::array<VkDescriptorPoolSize, 2> descriptorPoolSizes{};
		descriptorPoolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorPoolSizes[0].descriptorCount = static_cast<uint32_t>(cubes.size()) * maxConcurrentFrames;

		descriptorPoolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorPoolSizes[1].descriptorCount = static_cast<uint32_t>(cubes.size()) * maxConcurrentFrames;

		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
		descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
		descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();
		descriptorPoolCreateInfo.maxSets = static_cast<uint32_t>(cubes.size()) * maxConcurrentFrames;

		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool));


		// describes the shader bindings
		std::array<VkDescriptorSetLayoutBinding, 2> descriptorSetLayoutBindings{};

		// binding 0 = uniform buffers, used to pass matrices
		descriptorSetLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorSetLayoutBindings[0].binding = 0;
		descriptorSetLayoutBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		descriptorSetLayoutBindings[0].descriptorCount = 1;


		// combined image sampler
		descriptorSetLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorSetLayoutBindings[1].binding = 1;
		descriptorSetLayoutBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		descriptorSetLayoutBindings[1].descriptorCount = 1;

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
		descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
		descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings.data();

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout));

		// descriptor sets will be allocated, the descriptor sets contain the actual descriptor used at render time

		for(auto& cube : cubes)
		{
			for(auto i = 0; i < cube.uniformBuffers.size(); i++)
			{
				VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
				descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
				descriptorSetAllocateInfo.descriptorPool = descriptorPool;
				descriptorSetAllocateInfo.descriptorSetCount = 1;
				descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;
				VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &cube.uniformBuffers[i].descriptorSet));
			
				// updates the descriptor set with the actual descriptos matching shader binding that was set in layout
				std::array<VkWriteDescriptorSet, 2> writeDescriptorsSets{};

				VkDescriptorBufferInfo bufferInfo
				{
					.buffer = cube.uniformBuffers[i].handle,
					.range = sizeof(cube.matrices)
				};
				
				// binding 0 = object matrices uniform buffer
				writeDescriptorsSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorsSets[0].dstSet = cube.uniformBuffers[i].descriptorSet;
				writeDescriptorsSets[0].dstBinding = 0;
				writeDescriptorsSets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				writeDescriptorsSets[0].pBufferInfo = &bufferInfo;
				writeDescriptorsSets[0].descriptorCount = 1;

				// binding 0 = object texture
				writeDescriptorsSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorsSets[1].dstSet = cube.uniformBuffers[i].descriptorSet;
				writeDescriptorsSets[1].dstBinding = 1;
				writeDescriptorsSets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				writeDescriptorsSets[1].pImageInfo = &cube.texture.descriptor;
				writeDescriptorsSets[1].descriptorCount = 1;

				vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorsSets.size()), writeDescriptorsSets.data(), 0, nullptr);
			}
		}
	}

	void preparePipelines()
	{
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = 1,
			.pSetLayouts = &descriptorSetLayout
		};
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

		const std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_LINE_WIDTH };

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = vkt::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = vkt::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vkt::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = vkt::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = vkt::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportStateCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.scissorCount = 1
		};
		VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = vkt::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size()),
			.pDynamicStates = dynamicStateEnables.data()
		};
		VkPipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &swapChain.colorFormat,
			.depthAttachmentFormat = depthFormat,
			.stencilAttachmentFormat = depthFormat
		};
	
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};
		shaderStages[0] =
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = loadSPIRVShader(getShadersPath() + "descriptorsets/cube.vert.spv"),
			.pName  = "main"
		};
		assert(shaderStages[0].module != VK_NULL_HANDLE);

		shaderStages[1] =
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = loadSPIRVShader(getShadersPath() + "descriptorsets/cube.frag.spv"),
			.pName = "main"
		};
		assert(shaderStages[1].module != VK_NULL_HANDLE);


		VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext = &pipelineRenderingCreateInfo,
			.stageCount = static_cast<uint32_t>(shaderStages.size()),
			.pStages = shaderStages.data(),
			.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV, vkglTF::VertexComponent::Color }),
			.pInputAssemblyState = &inputAssemblyStateCreateInfo,
			.pViewportState = &viewportStateCreateInfo,
			.pRasterizationState = &rasterizationStateCreateInfo,
			.pMultisampleState = &multisampleStateCreateInfo,
			.pDepthStencilState = &depthStencilStateCreateInfo,
			.pColorBlendState = &colorBlendStateCreateInfo,
			.pDynamicState = &dynamicStateCreateInfo,
			.layout = pipelineLayout,
		};
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &pipeline));
	}

	void createUniformBuffers()
	{
		VkBufferCreateInfo bufferInfo = vkt::initializers::bufferCreateInfo(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(cubes[0].matrices));
		for(auto& cube : cubes)
		{
			for(auto& uniformBuffer : cube.uniformBuffers)
			{
				VK_CHECK_RESULT(vkCreateBuffer(device, &bufferInfo, nullptr, &uniformBuffer.handle));
				VkMemoryRequirements memoryReqs;
				vkGetBufferMemoryRequirements(device, uniformBuffer.handle, &memoryReqs);
				VkMemoryAllocateInfo memoryAllocateInfo = vkt::initializers::memoryAllocateInfo();
				memoryAllocateInfo.allocationSize = memoryReqs.size;
				memoryAllocateInfo.memoryTypeIndex = getMemoryTypeIndex(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

				VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &uniformBuffer.memory));
				VK_CHECK_RESULT(vkBindBufferMemory(device, uniformBuffer.handle, uniformBuffer.memory, 0));
				VK_CHECK_RESULT(vkMapMemory(device, uniformBuffer.memory, 0, sizeof(cubes[0].matrices), 0, (void**)&uniformBuffer.mapped));
			}
		}
	}

	void createSyncronizationPrimitives()
	{
		for(uint32_t i = 0; i < maxConcurrentFrames; i++)
		{
			VkFenceCreateInfo fenceCreateInfo = vkt::initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
			VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &waitFences[i]));
		}

		VkSemaphoreCreateInfo semaphoreCreateInfo = vkt::initializers::semaphoreCreateInfo();

		presentCompleteSemaphore.resize(maxConcurrentFrames);
		for(auto& semaphore : presentCompleteSemaphore)
		{
			VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphore));
		}

		renderCompleteSemaphore.resize(swapChain.images.size());
		for (auto& semaphore : renderCompleteSemaphore)
		{
			VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphore));
		}
	}

	void createCommandBuffers()
	{
		VkCommandPoolCreateInfo commandPoolCreateInfo = vkt::initializers::commandPoolCreateInfo();
		commandPoolCreateInfo.pNext = nullptr;
		commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		commandPoolCreateInfo.queueFamilyIndex = swapChain.queueNodeIndex;
		VK_CHECK_RESULT(vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool));

		VkCommandBufferAllocateInfo commandBufferAllocateInfo = vkt::initializers::commandBufferAllocateInfo(commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, maxConcurrentFrames);
		VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, commandBuffers.data()));
	}

	void updateUniformBuffers()
	{
		cubes[0].matrices.model = glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, 0.0f, 0.0f));
		cubes[1].matrices.model = glm::translate(glm::mat4(1.0f), glm::vec3(1.5f, 0.5f, 0.0f));

		for(auto& cube : cubes)
		{
			cube.matrices.projection = camera.matrices.perspective;
			cube.matrices.view = camera.matrices.view;
			cube.matrices.model = glm::rotate(cube.matrices.model, glm::radians(cube.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
			cube.matrices.model = glm::rotate(cube.matrices.model, glm::radians(cube.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
			cube.matrices.model = glm::rotate(cube.matrices.model, glm::radians(cube.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
			cube.matrices.model = glm::scale(cube.matrices.model, glm::vec3(0.25f));
			
			memcpy(cube.uniformBuffers[currentFrame].mapped, &cube.matrices, sizeof(cube.matrices));
		}
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		createSyncronizationPrimitives();
		createCommandBuffers();
		loadAssets();
		createUniformBuffers();
		setupDescriptors();
		preparePipelines();
		prepared = true;
	}

	void render()
	{
		if (!prepared)
		{
			return;
		}

		if(animate && !paused)
		{
			cubes[0].rotation.x += 2.5f * frameTimer;
			if(cubes[0].rotation.x > 360.0f)
			{
				cubes[0].rotation.x -= 360.0f;
			}
			cubes[1].rotation.y += 2.0f * frameTimer;
			if (cubes[1].rotation.y > 360.0f)
			{
				cubes[1].rotation.y -= 360.0f;
			}
		}

		updateOverlay();
		
		{
			vkWaitForFences(device, 1, &waitFences[currentFrame], VK_TRUE, UINT64_MAX);
			VK_CHECK_RESULT(vkResetFences(device, 1, &waitFences[currentFrame]));

			uint32_t imageIndex{ 0 };
			VkResult result = vkAcquireNextImageKHR(device, swapChain.swapChain, UINT64_MAX, presentCompleteSemaphore[currentFrame], VK_NULL_HANDLE, &imageIndex);
			if(result == VK_ERROR_OUT_OF_DATE_KHR)
			{
				windowResize();
				return;
			}
			else if((result != VK_SUCCESS) && (result != VK_SUBOPTIMAL_KHR))
			{
				throw "could not acquire next swap chain image";
			}

			updateUniformBuffers();

			vkResetCommandBuffer(commandBuffers[currentFrame], 0);
			VkCommandBufferBeginInfo commandBufferBeginInfo = vkt::initializers::commandBufferBeginInfo();
			const VkCommandBuffer commandBuffer = commandBuffers[currentFrame];
			VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

			vkt::tools::insertImageMemoryBarrier(commandBuffer,
				swapChain.images[imageIndex],
				0,
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VkImageSubresourceRange
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
			);

			vkt::tools::insertImageMemoryBarrier(commandBuffer,
				depthStencil.image,
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				VkImageSubresourceRange
				{ VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 }
			);

			VkRenderingAttachmentInfo colorAttachment
			{
				.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.imageView = swapChain.imageViews[imageIndex],
				.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.clearValue = 
				{
					.color = defaultClearColor
				}
			};

			VkRenderingAttachmentInfo depthStencilAttachment
			{
				.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.imageView = depthStencil.view,
				.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.clearValue =
				{
					.depthStencil = { 1.0f, 0 }
				}
			};

			VkRenderingInfo renderingInfo
			{
				.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
				.renderArea = { 0, 0, width, height},
				.layerCount = 1,
				.colorAttachmentCount = 1,
				.pColorAttachments = &colorAttachment,
				.pDepthAttachment = &depthStencilAttachment,
				.pStencilAttachment = &depthStencilAttachment
			};

			vkCmdBeginRendering(commandBuffer, &renderingInfo);
			VkViewport viewport = vkt::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

			VkRect2D scissor = vkt::initializers::rect2D(width, height, 0, 0);
			vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

			model.bindBuffers(commandBuffer);


			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
			for(auto cube : cubes)
			{
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &cube.uniformBuffers[currentFrame].descriptorSet, 0, nullptr);
				model.draw(commandBuffer);
			}

			drawUI(commandBuffer);

			vkCmdEndRendering(commandBuffer);

			vkt::tools::insertImageMemoryBarrier(commandBuffer,
				swapChain.images[imageIndex],
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				0,
				VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_2_NONE,
				VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1,0, 1 }
			);

			VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

			VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

			VkSubmitInfo submitInfo
			{
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &presentCompleteSemaphore[currentFrame],
				.pWaitDstStageMask = &waitStageMask,
				.commandBufferCount = 1,
				.pCommandBuffers = &commandBuffer,
				.signalSemaphoreCount = 1,
				.pSignalSemaphores = &renderCompleteSemaphores[imageIndex]
			};
			VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, waitFences[currentFrame]));

			VkPresentInfoKHR presentInfo
			{
				.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &renderCompleteSemaphores[imageIndex],
				.swapchainCount = 1,
				.pSwapchains = &swapChain.swapChain,
				.pImageIndices = &imageIndex
			};
			result = vkQueuePresentKHR(queue, &presentInfo);
			if((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR))
			{
				windowResize();
			}
			else if(result != VK_SUCCESS)
			{
				throw "could no present the image to the swap chain";
			}
			
			currentFrame = (currentFrame + 1) % maxConcurrentFrames;
		}
	}

	virtual void OnUpdateUIOverlay(vkt::UIOverlay* overlay)
	{
		if(overlay->header("Settings"))
		{
			overlay->checkBox("Animate", &animate);
		}
	}
public:
	bool animate = true;

	struct VulkanBuffeR
	{
		VkDeviceMemory memory{ VK_NULL_HANDLE };
		VkBuffer handle;
	};

	struct UniformBuffer : VulkanBuffeR
	{
		VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
		uint8_t* mapped{ nullptr };
	};

	struct Cube 
	{
		struct Matrices
		{
			glm::mat4 projection;
			glm::mat4 view;
			glm::mat4 model;
		} matrices;

		vkt::Texture2D texture;
		std::array<UniformBuffer, maxConcurrentFrames> uniformBuffers{};
		glm::vec3 rotation{ 0.0f };
	};

	std::array<Cube, 2> cubes;

	vkglTF::Model model;

	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	VkPipeline pipeline{ VK_NULL_HANDLE };
	VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };

	VkPhysicalDeviceVulkan13Features enabledVulkan13Features = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };

	std::vector<VkSemaphore> presentCompleteSemaphore{};
	std::vector<VkSemaphore> renderCompleteSemaphore{};

	std::array<VkFence, maxConcurrentFrames> waitFences;

	std::array<VkCommandBuffer, maxConcurrentFrames> commandBuffers;

	VkCommandPool commandPool{ VK_NULL_HANDLE };

	uint32_t currentFrame{ 0 };
};

VULKAN_EXAMPLE_MAIN()