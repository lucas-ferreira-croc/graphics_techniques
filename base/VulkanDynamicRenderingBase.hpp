#ifndef VULKAN_EXAMPLE_BASE_DYNAMIC_RENDERING
#define VULKAN_EXAMPLE_BASE_DYNAMIC_RENDERING

#include "vulkanexamplebase.h"

const int MAX_CONCURRENT_FRAMES = 2;
class VulkanExampleBaseDynamicRendering : public VulkanExampleBase
{
public:
	VulkanExampleBaseDynamicRendering()
		: VulkanExampleBase()
	{
		apiVersion = VK_API_VERSION_1_3;
		enabledVulkan13Features.dynamicRendering = VK_TRUE;
		enabledVulkan13Features.synchronization2 = VK_TRUE;
		deviceCreatepNextChain = &enabledVulkan13Features;
	}

	virtual void getEnabledFeatures() override
	{
		if (deviceProperties.apiVersion < VK_API_VERSION_1_3)
		{
			vkt::tools::exitFatal("selected gpou does not support vulkan 1.3", VK_ERROR_INCOMPATIBLE_DRIVER);
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

	void createSynchronizationPrimitives()
	{
		for (uint32_t i = 0; i < MAX_CONCURRENT_FRAMES; i++)
		{
			VkFenceCreateInfo fenceCreateInfo
			{
				.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
				.flags = VK_FENCE_CREATE_SIGNALED_BIT
			};
			VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &waitFences[i]));
		}

		VkSemaphoreCreateInfo semaphoreCreateInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		presentCompleteSemaphores.resize(MAX_CONCURRENT_FRAMES);
		for (auto& semaphore : presentCompleteSemaphores)
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

		VkCommandBufferAllocateInfo commandBufferAllocateInfo = vkt::initializers::commandBufferAllocateInfo(commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, MAX_CONCURRENT_FRAMES);
		VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, commandBuffers.data()));
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

public:

	struct VulkanBuffer
	{
		VkDeviceMemory memory{ VK_NULL_HANDLE };
		VkBuffer handle{ VK_NULL_HANDLE };
	};

	VulkanBuffer vertexBuffer;
	VulkanBuffer indexBuffer;
	uint32_t indexCount{ 0 };

	struct UniformBuffer : VulkanBuffer
	{
		// store the resources bound to the binding points in a shader;
		VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
		uint8_t* mapped{ nullptr };
	};

	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };

	VkPipeline pipeline{ VK_NULL_HANDLE };

	VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };

	std::vector<VkSemaphore> presentCompleteSemaphores{};
	std::vector<VkSemaphore> renderCompleteSemaphores{};

	std::array<VkFence, MAX_CONCURRENT_FRAMES> waitFences{};

	VkCommandPool commandPool{ VK_NULL_HANDLE };
	std::array<VkCommandBuffer, MAX_CONCURRENT_FRAMES> commandBuffers{};

	uint32_t currentFrame{ 0 };

	VkPhysicalDeviceVulkan13Features enabledVulkan13Features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
};

#endif