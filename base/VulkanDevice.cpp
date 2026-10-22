#include "VulkanDevice.h"

#include <unordered_set>

namespace vkt
{
	VulkanDevice::VulkanDevice(VkPhysicalDevice physicalDevice)
	{
		assert(physicalDevice);
		this->physicalDevice = physicalDevice;

		vkGetPhysicalDeviceProperties(physicalDevice, &properties);
		vkGetPhysicalDeviceFeatures(physicalDevice, &features);
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

		uint32_t queueFamilyCount;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
		assert(queueFamilyCount > 0);
		queueFamilyProperties.resize(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

		uint32_t extensionsCount = 0;
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionsCount, nullptr);
		if(extensionsCount > 0)
		{
			std::vector<VkExtensionProperties> extensions(extensionsCount);
			if(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionsCount, &extensions.front()) == VK_SUCCESS)
			{
				for(auto& extension : extensions)
				{
					supportedExtensions.push_back(extension.extensionName);
				}
			}
		}
	}

	VulkanDevice::~VulkanDevice()
	{
		if(commandPool)
		{
			vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
		}
		if(logicalDevice)
		{
			vkDestroyDevice(logicalDevice, nullptr);
		}
	}

	uint32_t VulkanDevice::getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32* memoryTypeFound) const
	{
		for(uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
		{
			if((typeBits & 1) == 1)
			{
				if((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
				{
					if(memoryTypeFound)
					{
						*memoryTypeFound = true;
					}
					return i;
				}
			}
			typeBits >>= 1;
		}

		if(memoryTypeFound)
		{
			*memoryTypeFound = false;
			return 0;
		}
		else
		{
			throw std::runtime_error("Could not find a matching memory type");
		}
	}

	uint32_t VulkanDevice::getQueueFamilyIndex(VkQueueFlags queueFlags) const
	{
		if((queueFlags & VK_QUEUE_COMPUTE_BIT) == queueFlags)
		{
			for(uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
			{
				if((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
				{
					return i;
				}
			}
		}

		if ((queueFlags & VK_QUEUE_TRANSFER_BIT) == queueFlags)
		{
			for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
			{
				if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) && (queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0 && (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
				{
					return i;
				}
			}
		}

		for(uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
		{
			if((queueFamilyProperties[i].queueFlags & queueFlags) == queueFlags)
			{
				return i;
			}
		}

		std::runtime_error("Could not find a matching queue family index");
	}

	VkResult VulkanDevice::createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char*> enabledExtensions, void* pNextChain, bool useSwapChain, VkQueueFlags requestedQueueTypes)
	{
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};
		const float defaultQueuePriority{ 0.0f };

		if(requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT)
		{
			queueFamilyIndices.graphics = getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
			VkDeviceQueueCreateInfo queueInfo
			{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex = queueFamilyIndices.graphics,
				.queueCount = 1,
				.pQueuePriorities = &defaultQueuePriority
			};
			queueCreateInfos.push_back(queueInfo);
		}
		else
		{
			queueFamilyIndices.graphics = 0;
		}

		if (requestedQueueTypes & VK_QUEUE_COMPUTE_BIT)
		{
			queueFamilyIndices.compute = getQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT);
			if (queueFamilyIndices.compute != queueFamilyIndices.graphics)
			{
				VkDeviceQueueCreateInfo queueInfo
				{
					.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.queueFamilyIndex = queueFamilyIndices.compute,
					.queueCount = 1,
					.pQueuePriorities = &defaultQueuePriority
				};
				queueCreateInfos.push_back(queueInfo);
			}
		}
		else
		{
			queueFamilyIndices.compute = queueFamilyIndices.graphics;
		}

		if (requestedQueueTypes & VK_QUEUE_TRANSFER_BIT)
		{
			queueFamilyIndices.transfer = getQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT);
			if ((queueFamilyIndices.transfer!= queueFamilyIndices.graphics) && (queueFamilyIndices.transfer != queueFamilyIndices.compute))
			{
				VkDeviceQueueCreateInfo queueInfo
				{
					.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.queueFamilyIndex = queueFamilyIndices.transfer,
					.queueCount = 1,
					.pQueuePriorities = &defaultQueuePriority
				};
				queueCreateInfos.push_back(queueInfo);
			}
		}
		else
		{
			queueFamilyIndices.transfer = queueFamilyIndices.graphics;
		}

		std::vector<const char*> deviceExtensions(enabledExtensions);
		if(useSwapChain)
		{
			deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		}

		VkDeviceCreateInfo deviceCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
			.pQueueCreateInfos = queueCreateInfos.data(),
			.pEnabledFeatures = &enabledFeatures
		};

		VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{};
		if(pNextChain)
		{
			physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			physicalDeviceFeatures2.features = enabledFeatures;
			physicalDeviceFeatures2.pNext = pNextChain;
			
			deviceCreateInfo.pEnabledFeatures = nullptr;
			deviceCreateInfo.pNext = &physicalDeviceFeatures2;
		}

		if(deviceExtensions.size() > 0)
		{
			for(const char* enabledExtension : deviceExtensions)
			{
				if(!extensionSupported(enabledExtension))
				{
					std::cerr << "enabled device extension \"" << enabledExtension << "\" is not present at device level\n";
				}
			}

			deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
			deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
		}

		this->enabledFeatures = enabledFeatures;

		VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice);
		if(result != VK_SUCCESS)
		{
			return result;
		}

		commandPool = createCommandPool(queueFamilyIndices.graphics);
		return result;
	}

	VkResult VulkanDevice::createBuffer(VkBufferCreateFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, VkBuffer* buffer, VkDeviceMemory* memory, void* data)
	{
		VkBufferCreateInfo bufferCreateInfo = vkt::initializers::bufferCreateInfo(usageFlags, size);
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK_RESULT(vkCreateBuffer(logicalDevice, &bufferCreateInfo, nullptr, buffer));

		VkMemoryRequirements memoryReqs;
		VkMemoryAllocateInfo memoryAllocateInfo = vkt::initializers::memoryAllocateInfo();
		vkGetBufferMemoryRequirements(logicalDevice, *buffer, &memoryReqs);
		
		memoryAllocateInfo.allocationSize = memoryReqs.size;
		memoryAllocateInfo.memoryTypeIndex = getMemoryType(memoryReqs.memoryTypeBits, memoryPropertyFlags);

		VkMemoryAllocateFlagsInfoKHR allocateFlagsInfo{};
		if(usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
		{
			allocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR;
			allocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
			memoryAllocateInfo.pNext = &allocateFlagsInfo;
		}
		VK_CHECK_RESULT(vkAllocateMemory(logicalDevice, &memoryAllocateInfo, nullptr, memory));

		if(data != nullptr)
		{
			void* mapped;
			VK_CHECK_RESULT(vkMapMemory(logicalDevice, *memory, 0, size, memoryPropertyFlags, &mapped));
			memcpy(mapped, data, size);

			if((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
			{
				VkMappedMemoryRange memoryRange
				{
					.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
					.memory = *memory,
					.size = size
				};
				vkFlushMappedMemoryRanges(logicalDevice, 1, &memoryRange);
			}
			vkUnmapMemory(logicalDevice, *memory);
		}

		VK_CHECK_RESULT(vkBindBufferMemory(logicalDevice, *buffer, *memory, 0));
		return VK_SUCCESS;
	}

	VkResult VulkanDevice::createBuffer(VkBufferCreateFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, vkt::Buffer* buffer, void* data)
	{
		buffer->device = logicalDevice;

		VkBufferCreateInfo bufferCreateInfo = vkt::initializers::bufferCreateInfo(usageFlags, size);
		VK_CHECK_RESULT(vkCreateBuffer(logicalDevice, &bufferCreateInfo, nullptr, &buffer->buffer));

		VkMemoryRequirements memoryReqs;
		VkMemoryAllocateInfo memoryAllocateInfo = vkt::initializers::memoryAllocateInfo();
		vkGetBufferMemoryRequirements(logicalDevice, buffer->buffer, &memoryReqs);

		memoryAllocateInfo.allocationSize = memoryReqs.size;
		memoryAllocateInfo.memoryTypeIndex = getMemoryType(memoryReqs.memoryTypeBits, memoryPropertyFlags);

		VkMemoryAllocateFlagsInfoKHR allocateFlagsInfo{};
		if (usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
		{
			allocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR;
			allocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
			memoryAllocateInfo.pNext = &allocateFlagsInfo;
		}
		VK_CHECK_RESULT(vkAllocateMemory(logicalDevice, &memoryAllocateInfo, nullptr, &buffer->memory));

		buffer->alignment = memoryReqs.alignment;
		buffer->size = size;
		buffer->usageFlags = usageFlags;
		buffer->memoryPropertyFlags = memoryPropertyFlags;

		if(data != nullptr)
		{
			VK_CHECK_RESULT(buffer->map());
			memcpy(buffer->mapped, data, size);
			if ((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
			{
				buffer->flush();
			}
			buffer->unmap();
		}

		buffer->setupDescriptor();

		return buffer->bind();
	}

	void VulkanDevice::copyBuffer(vkt::Buffer* src, vkt::Buffer* dst, VkQueue queue, VkBufferCopy* copyRegion)
	{
		assert(dst->size >= src->size);
		assert(src->buffer);

		VkCommandBuffer copyCommand = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy bufferCopy{};
		if (copyRegion == nullptr)
		{
			bufferCopy.size = src->size;
		}
		else
		{
			bufferCopy = *copyRegion;
		}

		vkCmdCopyBuffer(copyCommand, src->buffer, dst->buffer, 1, &bufferCopy);
		flushCommandBuffer(copyCommand, queue);
	}

	VkCommandPool VulkanDevice::createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags)
	{
		VkCommandPoolCreateInfo commandPoolCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = createFlags,
			.queueFamilyIndex = queueFamilyIndex
		};
		VkCommandPool commandPool;
		VK_CHECK_RESULT(vkCreateCommandPool(logicalDevice, &commandPoolCreateInfo, nullptr, &commandPool));
		return commandPool;
	}

	VkCommandBuffer VulkanDevice::createCommandBuffer(VkCommandBufferLevel level, VkCommandPool pool, bool begin)
	{
		VkCommandBufferAllocateInfo commandBufferAllocateInfo = vkt::initializers::commandBufferAllocateInfo(pool, level, 1);
		VkCommandBuffer commandBuffer;
		VK_CHECK_RESULT(vkAllocateCommandBuffers(logicalDevice, &commandBufferAllocateInfo, &commandBuffer));
		if(begin)
		{
			VkCommandBufferBeginInfo beginInfo = vkt::initializers::commandBufferBeginInfo();
			VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &beginInfo));
		}
		return commandBuffer;
	}

	VkCommandBuffer VulkanDevice::createCommandBuffer(VkCommandBufferLevel level, bool begin)
	{
		return createCommandBuffer(level, commandPool, begin);
	}

	void VulkanDevice::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free)
	{
		if(commandBuffer == VK_NULL_HANDLE)
		{
			return;
		}

		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		VkSubmitInfo submitInfo
		{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers = &commandBuffer
		};

		VkFenceCreateInfo fenceInfo = vkt::initializers::fenceCreateInfo(VK_FLAGS_NONE);
		VkFence fence;
		VK_CHECK_RESULT(vkCreateFence(logicalDevice, &fenceInfo, nullptr, &fence));
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence));
		VK_CHECK_RESULT(vkWaitForFences(logicalDevice, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));
		vkDestroyFence(logicalDevice, fence, nullptr);
		if(free)
		{
			vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
		}
	}

	void VulkanDevice::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free)
	{
		flushCommandBuffer(commandBuffer, queue, commandPool, free);
	}

	bool VulkanDevice::extensionSupported(std::string extension)
	{
		return (std::find(supportedExtensions.begin(), supportedExtensions.end(), extension) != supportedExtensions.end());
	}

	VkFormat VulkanDevice::getSupportedDepthFormat(bool checkSamplingSupport)
	{
		std::vector<VkFormat> depthFormats =
		{
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D16_UNORM_S8_UINT,
			VK_FORMAT_D16_UNORM
		};

		for(auto& format : depthFormats)
		{
			VkFormatProperties formatProperties;
			vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);
			if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
			{
				if(checkSamplingSupport)
				{
					if(!formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
					{
						continue;
					}
					return format;
				}
			}
		}
		throw std::runtime_error("could not find a matching depth format");
	}



}