#include "VulkanDynamicRenderingBase.hpp"

constexpr auto OBJECT_INSTANCES = 125;

struct Vertex
{
	float pos[3];
	float color[3];
};

void* alignedAlloc(size_t size, size_t aligment)
{
	void* data = nullptr;
#if defined(_MSC_VER) || defined(__MINGW32__)
	data = _aligned_malloc(size, aligment);
#endif
	return data;
}

void alignedFree(void* data)
{
#if defined(_MSC_VER) || defined(__MINGW32__)
	_aligned_free(data);
#endif
}

class VulkanExample : public VulkanExampleBaseDynamicRendering
{
public:
	VulkanExample()
		: VulkanExampleBaseDynamicRendering()
	{
		title = "Dynamic uniform buffers";

		camera.type = Camera::CameraType::lookat;
		camera.setPosition(glm::vec3(0.0f, 0.0f, -30.0f));
		camera.setRotation(glm::vec3(0.0f));
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);
	}

	~VulkanExample()
	{
		if(device)
		{
			if(uboDataDynamic.model)
			{
				alignedFree(uboDataDynamic.model);
			}
			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

			vkDestroyBuffer(device, vertexBuffer.handle, nullptr);
			vkDestroyBuffer(device, indexBuffer.handle, nullptr);
			vkFreeMemory(device, vertexBuffer.memory, nullptr);
			vkFreeMemory(device, indexBuffer.memory, nullptr);

			vkDestroyCommandPool(device, commandPool, nullptr);
			
			for(size_t i = 0; i < presentCompleteSemaphores.size(); i++)
			{
				vkDestroySemaphore(device, presentCompleteSemaphores[i], nullptr);
			}

			for (size_t i = 0; i < renderCompleteSemaphores.size(); i++)
			{
				vkDestroySemaphore(device, renderCompleteSemaphores[i], nullptr);
			}

			for(uint32_t i = 0; i < MAX_CONCURRENT_FRAMES; i++)
			{
				vkDestroyFence(device, waitFences[i], nullptr);
			}

			for(auto& buffer : uniformBuffers)
			{
				buffer.view.destroy();
				buffer.dynamic.destroy();
			}
		}
	}

	void generateCube()
	{
		std::vector <Vertex> vertices =
		{
			{ { -1.0f, -1.0f,  1.0f },{1.0f, 0.0f, 0.0f} },
			{ {  1.0f, -1.0f,  1.0f },{ 0.0f, 1.0f, 0.0f } },
			{ {  1.0f,  1.0f,  1.0f },{ 0.0f, 0.0f, 1.0f } },
			{ { -1.0f,  1.0f,  1.0f },{ 0.0f, 0.0f, 0.0f } },
			{ { -1.0f, -1.0f, -1.0f },{ 1.0f, 0.0f, 0.0f } },
			{ {  1.0f, -1.0f, -1.0f },{ 0.0f, 1.0f, 0.0f } },
			{ {  1.0f,  1.0f, -1.0f },{ 0.0f, 0.0f, 1.0f } },
			{ { -1.0f,  1.0f, -1.0f },{ 0.0f, 0.0f, 0.0f } }
		};
		uint32_t vertexBufferSize = static_cast<uint32_t>(vertices.size()) * sizeof(Vertex);

		std::vector<uint32_t> indices =
		{
			0,1,2, 2,3,0, 1,5,6, 6,2,1, 7,6,5, 5,4,7, 4,0,3, 3,7,4, 4,5,1, 1,0,4, 3,2,6, 6,7,3
		};

		indexCount = static_cast<uint32_t>(indices.size());
		uint32_t indexBufferSize = indexCount * sizeof(uint32_t);


		VulkanBuffer stagingBuffer;
		VkBufferCreateInfo stagingBufferCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = vertexBufferSize + indexBufferSize,
			.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
		};
		VK_CHECK_RESULT(vkCreateBuffer(device, &stagingBufferCreateInfo, nullptr, &stagingBuffer.handle));
		VkMemoryRequirements memoryReqs;
		vkGetBufferMemoryRequirements(device, stagingBuffer.handle, &memoryReqs);
		VkMemoryAllocateInfo memoryAllocInfo
		{
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = memoryReqs.size,
			.memoryTypeIndex = getMemoryTypeIndex(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
		};
		VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAllocInfo, nullptr, &stagingBuffer.memory));
		VK_CHECK_RESULT(vkBindBufferMemory(device, stagingBuffer.handle, stagingBuffer.memory, 0));

		uint8_t* data{ nullptr };
		VK_CHECK_RESULT(vkMapMemory(device, stagingBuffer.memory, 0, memoryAllocInfo.allocationSize, 0, (void**)&data));
		memcpy(data, vertices.data(), vertexBufferSize);
		memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

		VkBufferCreateInfo vertexBufferCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = vertexBufferSize,
			.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
		};
		VK_CHECK_RESULT(vkCreateBuffer(device, &vertexBufferCreateInfo, nullptr, &vertexBuffer.handle));
		vkGetBufferMemoryRequirements(device, vertexBuffer.handle, &memoryReqs);
		memoryAllocInfo.allocationSize = memoryReqs.size;
		memoryAllocInfo.memoryTypeIndex = getMemoryTypeIndex(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAllocInfo, nullptr, &vertexBuffer.memory));
		VK_CHECK_RESULT(vkBindBufferMemory(device, vertexBuffer.handle, vertexBuffer.memory, 0));

		VkBufferCreateInfo indexBufferCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = indexBufferSize,
			.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
		};
		VK_CHECK_RESULT(vkCreateBuffer(device, &indexBufferCreateInfo, nullptr, &indexBuffer.handle));
		vkGetBufferMemoryRequirements(device, indexBuffer.handle, &memoryReqs);
		memoryAllocInfo.allocationSize = memoryReqs.size;
		memoryAllocInfo.memoryTypeIndex = getMemoryTypeIndex(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAllocInfo, nullptr, &indexBuffer.memory));
		VK_CHECK_RESULT(vkBindBufferMemory(device, indexBuffer.handle, indexBuffer.memory, 0));

		VkCommandBuffer copyCommandBuffer;
		VkCommandBufferAllocateInfo commandBufferAllocateInfo
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = commandPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1
		};
		VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &copyCommandBuffer));

		VkCommandBufferBeginInfo commandBufferBeginInfo = vkt::initializers::commandBufferBeginInfo();
		VK_CHECK_RESULT(vkBeginCommandBuffer(copyCommandBuffer, &commandBufferBeginInfo));

		VkBufferCopy copyRegion{};
		copyRegion.size = vertexBufferSize;
		vkCmdCopyBuffer(copyCommandBuffer, stagingBuffer.handle, vertexBuffer.handle, 1, &copyRegion);

		copyRegion.size = indexBufferSize;
		copyRegion.srcOffset = vertexBufferSize;
		vkCmdCopyBuffer(copyCommandBuffer, stagingBuffer.handle, indexBuffer.handle, 1, &copyRegion);
		
		VK_CHECK_RESULT(vkEndCommandBuffer(copyCommandBuffer));

		VkSubmitInfo submitInfo
		{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers = &copyCommandBuffer
		};

		VkFenceCreateInfo fenceCreateInfo{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		VkFence fence;
		VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &fence));

		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence));

		VK_CHECK_RESULT(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));

		vkDestroyFence(device, fence, nullptr);
		vkFreeCommandBuffers(device, commandPool, 1, &copyCommandBuffer);

		vkDestroyBuffer(device, stagingBuffer.handle, nullptr);
		vkFreeMemory(device, stagingBuffer.memory, nullptr);
	}

	void createDescriptors()
	{
		std::vector<VkDescriptorPoolSize> poolSizes =
		{
			vkt::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_CONCURRENT_FRAMES),
			vkt::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, MAX_CONCURRENT_FRAMES)
		};
		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = vkt::initializers::descriptorPoolCreateInfo(poolSizes, MAX_CONCURRENT_FRAMES);
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool));

		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
		{
			vkt::initializers::vkDescriptorSetlayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
			vkt::initializers::vkDescriptorSetlayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT, 1)
		};
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = vkt::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout));

		// sets per frame, images dopest not need to be duplicated, we can reuse them 
		VkDescriptorSetAllocateInfo descriptorSetAllocInfo = vkt::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
		for(auto i = 0; i < uniformBuffers.size(); i++)
		{

			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocInfo, &descriptorSet));
			std::vector<VkWriteDescriptorSet> writeDescriptorsSets =
			{
				vkt::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers[currentFrame].view.descriptor),
				vkt::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, &uniformBuffers[currentFrame].dynamic.descriptor)
			};
			vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorsSets.size()), writeDescriptorsSets.data(), 0, nullptr);
		}
	}

	void createUniformBuffers()
	{
		size_t minUboAligment = vulkanDevice->properties.limits.minUniformBufferOffsetAlignment;
		dynamicAlignment = sizeof(glm::mat4);
		if(minUboAligment > 0)
		{
			dynamicAlignment = (dynamicAlignment + minUboAligment - 1) & ~(minUboAligment - 1);
		}

		size_t bufferSize = OBJECT_INSTANCES * dynamicAlignment;

		uboDataDynamic.model = (glm::mat4*)alignedAlloc(bufferSize, dynamicAlignment);
		assert(uboDataDynamic.model);

		std::cout << "minUniformBufferOffsetAligment = " << minUboAligment << std::endl;
		std::cout << "dynamicAligment = " << dynamicAlignment << std::endl;

		for(auto& buffer : uniformBuffers)
		{
			VK_CHECK_RESULT(vulkanDevice->createBuffer(
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				sizeof(uboVS),
				&buffer.view
			));

			VK_CHECK_RESULT(vulkanDevice->createBuffer(
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
				bufferSize,
				&buffer.dynamic
			));

			buffer.dynamic.descriptor.range = dynamicAlignment;

			VK_CHECK_RESULT(buffer.view.map());
			VK_CHECK_RESULT(buffer.dynamic.map());
		}

		std::default_random_engine randomEngine((unsigned)time(nullptr));
		std::normal_distribution<float> randomDistance(-1.0f, 1.0f);
		for(uint32_t i = 0; i < OBJECT_INSTANCES; i++)
		{
			rotations[i] = glm::vec3(randomDistance(randomEngine), randomDistance(randomEngine), randomDistance(randomEngine)) * 2.0f * (float)M_PI;
			rotationSpeeds[i] = glm::vec3(randomDistance(randomEngine), randomDistance(randomEngine), randomDistance(randomEngine));
		}
	}

	void updateUniformBuffers()
	{
		uboVS.projection = camera.matrices.perspective;
		uboVS.view = camera.matrices.view;
		memcpy(uniformBuffers[currentFrame].view.mapped, &uboVS, sizeof(uboVS));
	}

	void updateDynamicUniformBuffers()
	{
		uint32_t dimensions = static_cast<uint32_t>(pow(OBJECT_INSTANCES, (1.0f / 3.0f)));
		glm::vec3 offset(5.0f);
		for(uint32_t x = 0; x < dimensions; x++)
		{
			for(uint32_t y = 0; y < dimensions; y++)
			{
				for(uint32_t z = 0; z < dimensions; z++)
				{
					const uint32_t index = x * dimensions * dimensions + y * dimensions + z;

					glm::mat4* modelMatrix = (glm::mat4*)(((uint64_t)uboDataDynamic.model + (index * dynamicAlignment)));

					rotations[index] += frameTimer * rotationSpeeds[index];

					glm::vec3 pos = glm::vec3(
						-((dimensions * offset.x) / 2.0f) + offset.x / 2.0f + x * offset.x,
						-((dimensions * offset.y) / 2.0f) + offset.y / 2.0f + y * offset.y,
						-((dimensions * offset.z) / 2.0f) + offset.z / 2.0f + z * offset.z
					);
					*modelMatrix = glm::translate(glm::mat4(1.0f), pos);
					*modelMatrix = glm::rotate(*modelMatrix, rotations[index].x, glm::vec3(1.0f, 0.0f, 0.0f));
					*modelMatrix = glm::rotate(*modelMatrix, rotations[index].y, glm::vec3(0.0f, 1.0f, 0.0f));
					*modelMatrix = glm::rotate(*modelMatrix, rotations[index].x, glm::vec3(0.0f, 0.0f, 1.0f));
				}
			}
		}
		memcpy(uniformBuffers[currentFrame].dynamic.mapped, uboDataDynamic.model, uniformBuffers[currentFrame].dynamic.size);

		VkMappedMemoryRange memoryRange = vkt::initializers::mappedMemoryRange();
		memoryRange.memory = uniformBuffers[currentFrame].dynamic.memory;
		memoryRange.size = uniformBuffers[currentFrame].dynamic.size;
		vkFlushMappedMemoryRanges(device, 1, &memoryRange);
	}

	void preparePipelines()
	{
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vkt::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = vkt::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = vkt::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState colorBlendAttachmentState = vkt::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = vkt::initializers::pipelineColorBlendStateCreateInfo(1, &colorBlendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = vkt::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportStateCreateInfo = vkt::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = vkt::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnales = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vkt::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnales);

		// vertex binding and attributes
		VkVertexInputBindingDescription vertexInputBinding =
		{
			vkt::initializers::vertexInputBindingDescription(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX)
		};
		
		std::vector<VkVertexInputAttributeDescription> vertexInputAttributes
		{
			vkt::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)),
			vkt::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)),
		};

		VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = vkt::initializers::pipelineVertexInputStateCreateInfo();
		vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
		vertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexInputBinding;
		vertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
		vertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexInputAttributes.data();

		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};
		shaderStages[0] =
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = loadSPIRVShader(getShadersPath() + "dynamicuniformbuffers/base.vert.spv"),
			.pName = "main"
		};
		assert(shaderStages[0].module != VK_NULL_HANDLE);
		
		shaderStages[1] =
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = loadSPIRVShader(getShadersPath() + "dynamicuniformbuffers/base.frag.spv"),
			.pName = "main"
		};
		assert(shaderStages[1].module != VK_NULL_HANDLE);

		VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &swapChain.colorFormat,
			.depthAttachmentFormat = depthFormat,
			.stencilAttachmentFormat = depthFormat
		};

		VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext = &pipelineRenderingCreateInfo,
			.stageCount = static_cast<uint32_t>(shaderStages.size()),
			.pStages = shaderStages.data(),
			.pVertexInputState = &vertexInputStateCreateInfo,
			.pInputAssemblyState = &inputAssemblyStateCreateInfo,
			.pViewportState = &viewportStateCreateInfo,
			.pRasterizationState = &rasterizationStateCreateInfo,
			.pMultisampleState = &multisampleStateCreateInfo,
			.pDepthStencilState = &depthStencilStateCreateInfo,
			.pColorBlendState = &colorBlendStateCreateInfo,
			.pDynamicState = &dynamicState,
			.layout = pipelineLayout
		};
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1,&graphicsPipelineCreateInfo, nullptr, &pipeline));

		vkDestroyShaderModule(device, shaderStages[0].module, nullptr);
		vkDestroyShaderModule(device, shaderStages[1].module, nullptr);
	}

	void prepare() override
	{
		VulkanExampleBase::prepare();
		createSynchronizationPrimitives();
		createCommandBuffers();
		generateCube();
		createUniformBuffers();
		createDescriptors();
		preparePipelines();
		prepared = true;
	}

	virtual void render()
	{
		updateOverlay();
		updateUniformBuffers();
		updateDynamicUniformBuffers();
		{
			vkWaitForFences(device, 1, &waitFences[currentFrame], VK_TRUE, UINT64_MAX);
			VK_CHECK_RESULT(vkResetFences(device, 1, &waitFences[currentFrame]));

			uint32_t imageIndex{ 0 };
			VkResult result = vkAcquireNextImageKHR(device, swapChain.swapChain, UINT64_MAX, presentCompleteSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
			if(result == VK_ERROR_OUT_OF_DATE_KHR)
			{
				windowResize();
				return;
			}
			else if((result != VK_SUCCESS) && (result != VK_SUBOPTIMAL_KHR))
			{
				throw "could not acquire next swap chain image";
			}

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
					.depthStencil = {1.0f, 0}
				}
			};

			VkRenderingInfo renderingInfo
			{
				.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
				.renderArea = { 0, 0, width, height },
				.layerCount = 1,
				.colorAttachmentCount = 1,
				.pColorAttachments = &colorAttachment,
				.pDepthAttachment = &depthStencilAttachment,
				.pStencilAttachment = &depthStencilAttachment
			};

			vkCmdBeginRendering(commandBuffer, &renderingInfo);
			VkViewport viewport = vkt::initializers::viewport((float)width, (float)height, 0.f, 1.0f);
			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

			VkRect2D scissor = vkt::initializers::rect2D(width, height, 0, 0);
			vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.handle, offsets);
			vkCmdBindIndexBuffer(commandBuffer, indexBuffer.handle, 0, VK_INDEX_TYPE_UINT32);
			
			// render multiple objects using different omdel matrices by dynamically offsettings into one unifrom buffers
			for(uint32_t i = 0; i < OBJECT_INSTANCES; i++)
			{
				uint32_t dynamicOffset = i * static_cast<uint32_t>(dynamicAlignment);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 1, &dynamicOffset);

				vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
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
				.pWaitSemaphores = &presentCompleteSemaphores[currentFrame],
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
				throw "could not present the image to the swap chain";
			}

			currentFrame = (currentFrame + 1) & MAX_CONCURRENT_FRAMES;
		}
	}

public:
	VulkanBuffer vertexBuffer;
	VulkanBuffer indexBuffer;
	uint32_t indexCount{ 0 };

	struct UniformBuffers
	{
		vkt::Buffer view;
		vkt::Buffer dynamic;

	};
	std::array<UniformBuffers, MAX_CONCURRENT_FRAMES> uniformBuffers;

	VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };

	struct 
	{
		glm::mat4 projection;
		glm::mat4 view;
	} uboVS;

	// random per object rotation
	glm::vec3 rotations[OBJECT_INSTANCES];
	glm::vec3 rotationSpeeds[OBJECT_INSTANCES];

	// big uniform buffer that contains all matrices
	// manuallya llocate the data to cope for gpu-specific uniform buffer offset alignmetns
	struct UboDataDynamic
	{
		glm::mat4* model{ nullptr };
	} uboDataDynamic;

	size_t dynamicAlignment{ 0 };
};

VULKAN_EXAMPLE_MAIN()