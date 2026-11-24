#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

class VulkanExample :  public VulkanExampleBase
{
public:
	
	VulkanExample() : VulkanExampleBase()
	{
		title = "pipeline state objects";
		camera.type = Camera::CameraType::lookat;
		camera.setPosition(glm::vec3(0.0f, 0.0f, -10.5f));
		camera.setRotation(glm::vec3(-25.0f, 15.0f, 0.0f));
		camera.setRotationSpeed(0.5F);
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);

		apiVersion = VK_API_VERSION_1_3;
		enabledVulkan13Features.dynamicRendering = VK_TRUE;
		enabledVulkan13Features.synchronization2 = VK_TRUE;
		deviceCreatepNextChain = &enabledVulkan13Features;
	}

	~VulkanExample()
	{
		if(device)
		{
			vkDestroyPipeline(device, pipelines.phong, nullptr);
			if(enabledFeatures.fillModeNonSolid)
			{
				vkDestroyPipeline(device, pipelines.wireFrame, nullptr);
			}
			vkDestroyPipeline(device, pipelines.toon, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
			
			vkDestroyCommandPool(device, commandPool, nullptr);
			for (size_t i = 0; i < presentCompleteSemaphores.size(); i++)
			{
				vkDestroySemaphore(device, presentCompleteSemaphores[i], nullptr);
			}

			for (size_t i = 0; i < renderCompleteSemaphores.size(); i++)
			{
				vkDestroySemaphore(device, renderCompleteSemaphores[i], nullptr);
			}

			for(size_t i = 0; i < maxConcurrentFrames; i++)
			{
				vkDestroyFence(device, waitFences[i], nullptr);
				vkDestroyBuffer(device, uniformBuffers[i].handle, nullptr);
				vkFreeMemory(device, uniformBuffers[i].memory, nullptr);
			}

			
		}
	}

	virtual void getEnabledFeatures() override
	{
		if(deviceProperties.apiVersion < VK_API_VERSION_1_3)
		{
			vkt::tools::exitFatal("selected gpou does not support vulkan 1.3", VK_ERROR_INCOMPATIBLE_DRIVER);
		}

		if(deviceFeatures.fillModeNonSolid)
		{
			enabledFeatures.fillModeNonSolid = VK_TRUE;
		}

		if(deviceFeatures.wideLines)
		{
			enabledFeatures.wideLines = VK_TRUE;
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

	void loadAssets()
	{
		const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
		scene.loadFromFile(getAssetPath() + "models/treasure_smooth.gltf", vulkanDevice, queue, glTFLoadingFlags);
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

	void setupDescriptors()
	{
		VkDescriptorPoolSize descriptorTypeCounts[1]{};
		descriptorTypeCounts[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorTypeCounts[0].descriptorCount = maxConcurrentFrames;

		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.maxSets = maxConcurrentFrames,
			.poolSizeCount = 1,
			.pPoolSizes = descriptorTypeCounts
		};
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool));

		VkDescriptorSetLayoutBinding layoutBinding
		{
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
		};

		VkDescriptorSetLayoutCreateInfo descriptorLayoutCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = 1,
			.pBindings = &layoutBinding
		};
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayoutCreateInfo, nullptr, &descriptorSetLayout));
	
		for (uint32_t i = 0; i < maxConcurrentFrames; i++)
		{
			VkDescriptorSetAllocateInfo descriptorSetAllocInfo
			{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = descriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &descriptorSetLayout
			};
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocInfo, &uniformBuffers[i].descriptorSet));

			VkDescriptorBufferInfo bufferInfo
			{
				.buffer = uniformBuffers[i].handle,
				.range = sizeof(UniformData)
			};

			VkWriteDescriptorSet writeDescriptorSet
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = uniformBuffers[i].descriptorSet,
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.pBufferInfo = &bufferInfo
			};

			vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
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

	void preparePipelines()
	{
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = 1,
			.pSetLayouts = &descriptorSetLayout
		};
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
		};

		VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = vkt::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState{};
		blendAttachmentState.colorWriteMask = 0xf;
		blendAttachmentState.blendEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = 1,
			.pAttachments = &blendAttachmentState
		};

		VkPipelineViewportStateCreateInfo viewportStateCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.scissorCount = 1
		};

		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_LINE_WIDTH };
		VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size()),
			.pDynamicStates = dynamicStateEnables.data()
		};

		VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = VK_TRUE,
			.depthWriteEnable = VK_TRUE,
			.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
			.back = { .compareOp = VK_COMPARE_OP_ALWAYS }
		};

		VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
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
			.module = loadSPIRVShader(getShadersPath() + "pipelines/phong.vert.spv"),
			.pName = "main"
		};
		assert(shaderStages[0].module != VK_NULL_HANDLE);

		shaderStages[1] =
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = loadSPIRVShader(getShadersPath() + "pipelines/phong.frag.spv"),
			.pName = "main"
		};
		assert(shaderStages[1].module != VK_NULL_HANDLE);

		VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext = &pipelineRenderingCreateInfo,
			.stageCount = static_cast<uint32_t>(shaderStages.size()),
			.pStages = shaderStages.data(),
			.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Color }),
			.pInputAssemblyState = &inputAssemblyStateCreateInfo,
			.pViewportState = &viewportStateCreateInfo,
			.pRasterizationState = &rasterizationStateCreateInfo,
			.pMultisampleState = &multisampleStateCreateInfo,
			.pDepthStencilState = &depthStencilStateCreateInfo,
			.pColorBlendState = &colorBlendStateCreateInfo,
			.pDynamicState = &dynamicStateCreateInfo,
			.layout = pipelineLayout,
		};
		graphicsPipelineCreateInfo.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
		// textured phong shading pipeline

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &pipelines.phong));

		graphicsPipelineCreateInfo.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
		graphicsPipelineCreateInfo.basePipelineHandle = pipelines.phong;
		graphicsPipelineCreateInfo.basePipelineIndex = -1; // we can only use a handle or a index to base our pipeline, since we are using handle, index should be -1
		
		shaderStages[0] =
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = loadSPIRVShader(getShadersPath() + "pipelines/toon.vert.spv"),
			.pName = "main"
		};
		assert(shaderStages[0].module != VK_NULL_HANDLE);

		shaderStages[1] =
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = loadSPIRVShader(getShadersPath() + "pipelines/toon.frag.spv"),
			.pName = "main"
		};
		assert(shaderStages[1].module != VK_NULL_HANDLE);

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &pipelines.toon));

		if(enabledFeatures.fillModeNonSolid)
		{
			rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_LINE;
			shaderStages[0] =
			{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_VERTEX_BIT,
				.module = loadSPIRVShader(getShadersPath() + "pipelines/wireframe.vert.spv"),
				.pName = "main"
			};
			assert(shaderStages[0].module != VK_NULL_HANDLE);

			shaderStages[1] =
			{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
				.module = loadSPIRVShader(getShadersPath() + "pipelines/wireframe.frag.spv"),
				.pName = "main"
			};
			assert(shaderStages[1].module != VK_NULL_HANDLE);


			VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &pipelines.wireFrame));
		}
	}

	void createUniformBuffers()
	{
		VkBufferCreateInfo bufferInfo = vkt::initializers::bufferCreateInfo(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(UniformData));

		for(uint32_t i = 0; i < maxConcurrentFrames; i++)
		{
			VK_CHECK_RESULT(vkCreateBuffer(device, &bufferInfo, nullptr, &uniformBuffers[i].handle));
			VkMemoryRequirements memoryReqs;
			vkGetBufferMemoryRequirements(device, uniformBuffers[i].handle, &memoryReqs);
			VkMemoryAllocateInfo memoryAllocateInfo = vkt::initializers::memoryAllocateInfo();
			memoryAllocateInfo.allocationSize = memoryReqs.size;
			memoryAllocateInfo.memoryTypeIndex = getMemoryTypeIndex(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			
			VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &uniformBuffers[i].memory));
			VK_CHECK_RESULT(vkBindBufferMemory(device, uniformBuffers[i].handle, uniformBuffers[i].memory, 0));
			VK_CHECK_RESULT(vkMapMemory(device, uniformBuffers[i].memory, 0, sizeof(UniformData), 0, (void**)&uniformBuffers[i].mapped));
		}
	}

	void createSyncrhonizationPrimitives()
	{
		for(uint32_t i = 0; i < maxConcurrentFrames; i++)
		{
			VkFenceCreateInfo fenceCreateInfo = vkt::initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
			VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &waitFences[i]));
		}
		
		VkSemaphoreCreateInfo semaphoreCreateInfo = vkt::initializers::semaphoreCreateInfo();
		
		presentCompleteSemaphores.resize(maxConcurrentFrames);
		for(auto& semaphore : presentCompleteSemaphores)
		{
			VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphore));
		}

		renderCompleteSemaphores.resize(swapChain.images.size());
		for (auto& semaphore : renderCompleteSemaphores)
		{
			VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphore));
		}

	}

	void createCommandBuffers()
	{
		VkCommandPoolCreateInfo commandPoolCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = swapChain.queueNodeIndex
		};

		VK_CHECK_RESULT(vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool));

		VkCommandBufferAllocateInfo commandBufferAllocateInfo = vkt::initializers::commandBufferAllocateInfo(commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, maxConcurrentFrames);
		VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, commandBuffers.data()));
	}

	void prepare() override
	{
		VulkanExampleBase::prepare();
		createSyncrhonizationPrimitives();
		createCommandBuffers();
		loadAssets();
		createUniformBuffers();
		setupDescriptors();
		preparePipelines();
		prepared = true;
	}

	void render() override
	{
		updateOverlay();

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
			else if ((result != VK_SUCCESS) && (result != VK_SUBOPTIMAL_KHR))
			{
				throw "could not acquire next swap chain image";
			}

			camera.setPerspective(60.0f, (float)(width / 3.0f) / (float)height, 0.1f, 256.0f);
			uniformData.projection = camera.matrices.perspective;
			uniformData.modelView = camera.matrices.view;
			memcpy(uniformBuffers[currentFrame].mapped, &uniformData, sizeof(UniformData));

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
				.clearValue = {
					.color = defaultClearColor,
				}
			};

			VkRenderingAttachmentInfo depthStencilAttachment
			{
				.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.imageView = depthStencil.view,
				.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.clearValue = {
					.depthStencil = { 1.0f, 0 }
				}
			};

			VkRenderingInfo renderingInfo
			{
				.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
				.renderArea = { 0, 0, width, height },
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

			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &uniformBuffers[currentFrame].descriptorSet, 0, nullptr);
			scene.bindBuffers(commandBuffer);

			viewport.width = (float)width / 3.0f;
			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.phong);
			vkCmdSetLineWidth(commandBuffer, 1.0f);
			scene.draw(commandBuffer);

			viewport.x = (float)width / 3.0f;
			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.toon);
			if(enabledFeatures.wideLines)
			{
				vkCmdSetLineWidth(commandBuffer, 2.0f);
			}
			scene.draw(commandBuffer);
			
			if(enabledFeatures.fillModeNonSolid)
			{
				viewport.x = (float)width / 3.0f + (float)width / 3.0f;
				vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.wireFrame);
				scene.draw(commandBuffer);
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
			if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR))
			{
				windowResize();
			}
			else if (result != VK_SUCCESS)
			{
				throw "could not present the image to the swap chain";
			}

			currentFrame = (currentFrame + 1) % maxConcurrentFrames;
		}
	}

	virtual void OnUpdateUIOverlay(vkt::UIOverlay* overlay)
	{
		if(!enabledFeatures.fillModeNonSolid)
		{
			if(overlay->header("info"))
			{
				overlay->text("Non solid fill modes not supported");
			}
		}
	}


public:
	vkglTF::Model scene;

	struct UniformData
	{
		glm::mat4 projection;
		glm::mat4 modelView;
		glm::vec4 lighPosition{ 0.0f, 2.0f, 1.0f, 0.0f };
	} uniformData;

	struct VulkanBuffer
	{
		VkDeviceMemory memory{ VK_NULL_HANDLE };
		VkBuffer handle{ VK_NULL_HANDLE };
	};

	struct UniformBuffer : VulkanBuffer
	{
		VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
		uint8_t* mapped{ nullptr };
	};
	std::array<UniformBuffer, maxConcurrentFrames> uniformBuffers;

	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };

	struct
	{
		VkPipeline phong{ VK_NULL_HANDLE };
		VkPipeline wireFrame{ VK_NULL_HANDLE };
		VkPipeline toon{ VK_NULL_HANDLE };
	} pipelines;

	VkPhysicalDeviceVulkan13Features enabledVulkan13Features = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };

	std::vector<VkSemaphore> presentCompleteSemaphores{};
	std::vector<VkSemaphore> renderCompleteSemaphores{};

	std::array<VkFence, maxConcurrentFrames> waitFences{};

	std::array<VkCommandBuffer, maxConcurrentFrames> commandBuffers;

	VkCommandPool commandPool{ VK_NULL_HANDLE };


	uint32_t currentFrame{ 0 };
};

VULKAN_EXAMPLE_MAIN()