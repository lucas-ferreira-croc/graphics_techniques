#ifndef VULKAN_DEVICE_H
#define VULKAN_DEVICE_H

// encapsulates a vulkan device

#include "VulkanBuffer.h"
#include "VulkanTools.h"
#include <vulkan/vulkan.h>
#include <algorithm>
#include <assert.h>
#include <exception>

namespace vkt
{
	struct VulkanDevice
	{
		VkPhysicalDevice physicalDevice{ VK_NULL_HANDLE };
		VkDevice logicalDevice{ VK_NULL_HANDLE };
		VkPhysicalDeviceProperties properties{};
		VkPhysicalDeviceFeatures features{};
		VkPhysicalDeviceFeatures enabledFeatures{};
		VkPhysicalDeviceMemoryProperties memoryProperties{};

		std::vector<VkQueueFamilyProperties> queueFamilyProperties{};

		std::vector<std::string> supportedExtensions{};

		VkCommandPool commandPool{ VK_NULL_HANDLE };

		struct
		{
			uint32_t graphics;
			uint32_t compute;
			uint32_t transfer;
		} queueFamilyIndices;

		operator VkDevice() const
		{
			return logicalDevice;
		}

		explicit VulkanDevice(VkPhysicalDevice physicalDevice);
		~VulkanDevice();

		uint32_t getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32* memoryTypeFound = nullptr) const;
		uint32_t getQueueFamilyIndex(VkQueueFlags queueFlags) const;
		VkResult createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char*> enabledExtensions, void* pNextChain, bool useSwapChain = true, VkQueueFlags requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
		VkResult createBuffer(VkBufferCreateFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, VkBuffer* buffer, VkDeviceMemory* memory, void* data = nullptr);
		VkResult createBuffer(VkBufferCreateFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, vkt::Buffer* buffer, void* data = nullptr);
		void copyBuffer(vkt::Buffer* src, vkt::Buffer* dst, VkQueue queue, VkBufferCopy* copyRegion = nullptr);
		VkCommandPool createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
		VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, VkCommandPool pool, bool begin = false);
		VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin = false);
		void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free = true);
		void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free = true);
		bool extensionSupported(std::string extension);
		VkFormat getSupportedDepthFormat(bool checkSamplingSupport);
	};
}


#endif