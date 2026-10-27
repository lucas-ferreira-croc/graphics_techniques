#ifndef VULKAN_SWAP_CHAIN_H
#define VULKAN_SWAP_CHAIN_H

// wrapper to a swap chain

#include <stdlib.h>
#include <string>
#include <assert.h>
#include <stdio.h>
#include <vector>

#include <vulkan/vulkan.h>

#include "VulkanTools.h"

class VulkanSwapChain
{
public:
	VkFormat colorFormat{};
	VkColorSpaceKHR colorSpace{};
	VkSwapchainKHR swapChain{ VK_NULL_HANDLE };
	std::vector<VkImage> images{};
	std::vector<VkImageView> imageViews{};
	uint32_t queueNodeIndex{ UINT32_MAX };
	uint32_t imageCount{ 0 };

	void initSurface(void* platformHandle, void* platformWindow);

	void setContext(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);

	void create(uint32_t& width, uint32_t& height, bool vsync = false, bool fullscren = false);

	VkResult acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t& imageIndex);

	void cleanup();
private:
	VkInstance instance{ VK_NULL_HANDLE };
	VkDevice device{ VK_NULL_HANDLE };
	VkPhysicalDevice physicalDevice{ VK_NULL_HANDLE };
	VkSurfaceKHR surface{ VK_NULL_HANDLE };
};

#endif
