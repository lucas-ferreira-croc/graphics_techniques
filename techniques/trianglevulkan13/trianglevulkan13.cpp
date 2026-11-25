#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <exception>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>

#include "VulkanDynamicRenderingBase.hpp"

class VulkanExample : public VulkanExampleBaseDynamicRendering
{
public:
	struct Vertex
	{
		float position[3];
		float color[3];
	};
	
	struct ShaderData
	{
		glm::mat4 projectionMatrix;
		glm::mat4 modelMatrix;
		glm::mat4 viewMatrix;
	};

	std::array<UniformBuffer, MAX_CONCURRENT_FRAMES> uniformBuffers;

	VulkanExample() 
		: VulkanExampleBaseDynamicRendering()
	{
		title = "basic indexed triangle using vulkan 1.3";
		settings.overlay = true;

		camera.type = Camera::CameraType::lookat;
		camera.setPosition(glm::vec3(0.0f, 0.0f, -2.5f));
		camera.setRotation(glm::vec3(0.0f));
		camera.setPerspective(60.0f, (float)width / (float)height, 1.0f, 256.0f);
	}

	~VulkanExample() override
	{
		if(device)
		{
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
				vkDestroyBuffer(device, uniformBuffers[i].handle, nullptr);
				vkFreeMemory(device, uniformBuffers[i].memory, nullptr);
			}
		}
	}

	void createVertexBuffer()
	{
		const std::vector<Vertex> vertices
		{
			{ {  1.0f, 1.0f, 0.0f },  { 1.0f, 0.0f, 0.0f } },
			{ { -1.0f, 1.0f, 0.0f },  { 0.0f, 1.0f, 0.0f } },
			{ {  0.0f, -1.0f, 0.0f },  { 0.0f, 0.0f, 1.0f } }
		};
		uint32_t vertexBufferSize = static_cast<uint32_t>(vertices.size()) * sizeof(Vertex);

		std::vector<uint32_t> indices{ 0, 1, 2 };
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
		memcpy(((char*)data) + vertexBufferSize, indices.data(), indexBufferSize);

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

		VK_CHECK_RESULT(vkWaitForFences(device, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));
		vkDestroyFence(device, fence, nullptr);
		vkFreeCommandBuffers(device, commandPool, 1, &copyCommandBuffer);

		vkDestroyBuffer(device, stagingBuffer.handle, nullptr);
		vkFreeMemory(device, stagingBuffer.memory, nullptr);
	}

	void createDescriptors()
	{
		VkDescriptorPoolSize descriptorTypeCounts[1]{};
		descriptorTypeCounts[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorTypeCounts[0].descriptorCount = MAX_CONCURRENT_FRAMES;

		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.maxSets = MAX_CONCURRENT_FRAMES,
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
			.bindingCount =1,
			.pBindings = &layoutBinding
		};
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayoutCreateInfo, nullptr, &descriptorSetLayout));

		for(uint32_t i = 0; i < MAX_CONCURRENT_FRAMES; i++)
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
				.range = sizeof(ShaderData)
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

	void createPipeline()
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

		VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.depthClampEnable = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE,
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_NONE,
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
			.depthBiasEnable = VK_FALSE,
			.lineWidth = 1.0f
		};

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

		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
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
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable = VK_FALSE,
			.front = {.failOp = VK_STENCIL_OP_KEEP, .passOp = VK_STENCIL_OP_KEEP, .compareOp = VK_COMPARE_OP_ALWAYS},
			.back = { .failOp = VK_STENCIL_OP_KEEP, .passOp = VK_STENCIL_OP_KEEP, .compareOp = VK_COMPARE_OP_ALWAYS},
		};

		VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
		};

		VkVertexInputBindingDescription vertexinputBindingDescription
		{
			.binding = 0,
			.stride = sizeof(Vertex),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
		};

		std::array<VkVertexInputAttributeDescription, 2> vertexInputAttribs{};
		vertexInputAttribs[0].binding = 0;
		vertexInputAttribs[0].location = 0;
		vertexInputAttribs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertexInputAttribs[0].offset = offsetof(Vertex, position);

		vertexInputAttribs[1].binding = 0;
		vertexInputAttribs[1].location = 1;
		vertexInputAttribs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertexInputAttribs[1].offset = offsetof(Vertex, color);

		VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &vertexinputBindingDescription,
			.vertexAttributeDescriptionCount = 2,
			.pVertexAttributeDescriptions = vertexInputAttribs.data()
		};

		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};

		shaderStages[0] =
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = loadSPIRVShader(getShadersPath() + "trianglevulkan13/triangle.vert.spv"),
			.pName = "main"
		};
		assert(shaderStages[0].module != VK_NULL_HANDLE);

		shaderStages[1] =
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = loadSPIRVShader(getShadersPath() + "trianglevulkan13/triangle.frag.spv"),
			.pName = "main"
		};
		assert(shaderStages[1].module != VK_NULL_HANDLE);

		VkPipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo
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
			.pDynamicState = &dynamicStateCreateInfo,
			.layout = pipelineLayout
		};
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &pipeline));
		
		vkDestroyShaderModule(device, shaderStages[0].module, nullptr);
		vkDestroyShaderModule(device, shaderStages[1].module, nullptr);
	}

	void createUniformBuffers()
	{
		VkBufferCreateInfo bufferInfo
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = sizeof(ShaderData),
			.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
		};

		for(uint32_t i = 0; i < MAX_CONCURRENT_FRAMES; i++)
		{
			VK_CHECK_RESULT(vkCreateBuffer(device, &bufferInfo, nullptr, &uniformBuffers[i].handle));
			VkMemoryRequirements memoryReqs;
			vkGetBufferMemoryRequirements(device, uniformBuffers[i].handle, &memoryReqs);
			VkMemoryAllocateInfo memoryAllocateInfo
			{
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.allocationSize = memoryReqs.size,
				.memoryTypeIndex = getMemoryTypeIndex(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
			};
			VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &uniformBuffers[i].memory));
			VK_CHECK_RESULT(vkBindBufferMemory(device, uniformBuffers[i].handle, uniformBuffers[i].memory, 0));
			VK_CHECK_RESULT(vkMapMemory(device, uniformBuffers[i].memory, 0, sizeof(ShaderData), 0, (void**)&uniformBuffers[i].mapped));
		}
	}

	void prepare() override
	{
		VulkanExampleBase::prepare();
		createSynchronizationPrimitives();
		createCommandBuffers();
		createVertexBuffer();
		createUniformBuffers();
		createDescriptors();
		createPipeline();
		prepared = true;
	}

	void render() override
	{
		updateOverlay();
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

		ShaderData shaderData
		{
			.projectionMatrix = camera.matrices.perspective,
			.modelMatrix = glm::mat4(1.0f),
			.viewMatrix = camera.matrices.view
		};
		memcpy(uniformBuffers[currentFrame].mapped, &shaderData, sizeof(ShaderData));
		
		vkResetCommandBuffer(commandBuffers[currentFrame], 0);
		VkCommandBufferBeginInfo commandBufferInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		const VkCommandBuffer commandBuffer = commandBuffers[currentFrame];
		VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffers[currentFrame], &commandBufferInfo));

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
			0,
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
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
				.color = { 0.0f, 0.0f, 0.2f, 1.0f},
			}
		};

		VkRenderingAttachmentInfo depthStencilAttachment
		{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = depthStencil.view,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
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
			.pColorAttachments= &colorAttachment,
			.pDepthAttachment = &depthStencilAttachment,
		};

		vkCmdBeginRendering(commandBuffer, &renderingInfo);

		VkViewport viewport{ 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor{ 0, 0, width, height };
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &uniformBuffers[currentFrame].descriptorSet, 0, nullptr);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		VkDeviceSize offsets[1]{ 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.handle, offsets);
		vkCmdBindIndexBuffer(commandBuffer, indexBuffer.handle, 0, VK_INDEX_TYPE_UINT32);

		vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
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

		currentFrame = (currentFrame + 1) % MAX_CONCURRENT_FRAMES;
	}

	virtual void setupFramebuffer() {}
	virtual void setupRenderPass() {}
};

#if defined(_WIN32)
VulkanExample* vulkanExample;
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (vulkanExample != NULL) 
	{
		vulkanExample->handleMessages(hWnd, uMsg, wParam, lParam);
	}
	return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}

int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR, _In_ int)
{
	for(size_t i = 0; i < __argc; i++)
	{
		VulkanExample::args.push_back(__argv[i]);
	}
	vulkanExample = new VulkanExample();
	vulkanExample->initVulkan();
	vulkanExample->setupWindow(hInstance, WndProc);
	vulkanExample->prepare();
	vulkanExample->renderLoop();
	delete(vulkanExample);
	return 0;
}

#endif