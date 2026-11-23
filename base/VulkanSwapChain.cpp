#include "VulkanSwapChain.h"


// platformSpecific function, would need lots of other overloads to account for other OS'es
// currently supporting only windows
void VulkanSwapChain::initSurface(void* platformHandle, void* platformWindow)
{
	VkResult err = VK_SUCCESS;
	
	// 
	VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{};
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.hinstance = (HINSTANCE)platformHandle;
	surfaceCreateInfo.hwnd = (HWND)platformWindow;
	err = vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfo, nullptr, &surface);

	if(err != VK_SUCCESS)
	{
		vkt::tools::exitFatal("could not create surface", err);
	}

	uint32_t queueCount;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, nullptr);
	assert(queueCount >= 1);
	std::vector<VkQueueFamilyProperties> queueProperties(queueCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queueProperties.data());
	
	std::vector<VkBool32> supportsPresent(queueCount);
	for(uint32_t i = 0; i < queueCount; i++)
	{
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &supportsPresent[i]);
	}

	uint32_t graphicsQueueNodeIndex = UINT32_MAX;
	uint32_t presentQueueNodeIndex = UINT32_MAX;
	for(uint32_t i = 0; i < queueCount; i++)
	{
		if((queueProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
		{
			if(graphicsQueueNodeIndex == UINT32_MAX)
			{
				graphicsQueueNodeIndex = i;
			}
			if(supportsPresent[i] == VK_TRUE)
			{
				graphicsQueueNodeIndex = i;
				presentQueueNodeIndex = i;
				break;
			}
		}
	}

	if(presentQueueNodeIndex == UINT32_MAX)
	{
		for(uint32_t i = 0; i < queueCount; i++)
		{
			if (supportsPresent[i] == VK_TRUE)
			{
				presentQueueNodeIndex = i;
				break;
			}
		}
	}

	if(graphicsQueueNodeIndex == UINT32_MAX || presentQueueNodeIndex == UINT32_MAX)
	{
		vkt::tools::exitFatal("could not find a graphics and/or present queue", -1);
	}
	if(graphicsQueueNodeIndex != presentQueueNodeIndex)
	{
		vkt::tools::exitFatal("separete graphics and presenting queues are not supported, yet", -1);
	}
	queueNodeIndex = graphicsQueueNodeIndex;

	uint32_t formatCount;
	VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, NULL));
	assert(formatCount >= 1);
	std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
	VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, surfaceFormats.data()));

	VkSurfaceFormatKHR selectedFormat = surfaceFormats[0];
	std::vector<VkFormat> preferredImageFormats =
	{
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_A8B8G8R8_UNORM_PACK32,
	};
	for(auto& availableFormat : surfaceFormats)
	{
		if(std::find(preferredImageFormats.begin(), preferredImageFormats.end(), availableFormat.format) != preferredImageFormats.end())
		{
			selectedFormat = availableFormat;
			break;
		}
	}
	colorFormat = selectedFormat.format;
	colorSpace = selectedFormat.colorSpace;
}

void VulkanSwapChain::setContext(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device)
{
	this->instance = instance;
	this->physicalDevice = physicalDevice;
	this->device = device;
}

void VulkanSwapChain::create(uint32_t& width, uint32_t& height, bool vsync, bool fullscren)
{
	assert(physicalDevice);
	assert(device);
	assert(instance);

	VkSwapchainKHR oldSwapChain = swapChain;

	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities));

	VkExtent2D swapChainExtent{};

	if(surfaceCapabilities.currentExtent.width == (uint32_t)-1)
	{
		swapChainExtent.width = width;
		swapChainExtent.height = height;
	}
	else
	{
		swapChainExtent = surfaceCapabilities.currentExtent;
		width = surfaceCapabilities.currentExtent.width;
		height = surfaceCapabilities.currentExtent.height;
	}

	uint32_t presentModeCount;
	VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr));
	assert(presentModeCount > 0);

	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data()));

	// VK_PRESENT_MODE_FIFO_KHR always present per spec
	VkPresentModeKHR swapChainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

	if(!vsync)
	{
		for(size_t i = 0; i < presentModeCount; i++)
		{
			if(presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				swapChainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
				break;
			}
			if(presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
			{
				swapChainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
				break;
			}
		}
	}

	uint32_t desiredNumberOfSwapChainImages = surfaceCapabilities.minImageCount + 1;
	if((surfaceCapabilities.maxImageCount > 0) && (desiredNumberOfSwapChainImages > surfaceCapabilities.maxImageCount))
	{
		desiredNumberOfSwapChainImages = surfaceCapabilities.maxImageCount;
	}

	VkSurfaceTransformFlagsKHR preTransform;
	if(surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
	{
		preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else
	{
		preTransform = surfaceCapabilities.currentTransform;
	}

	VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags =
	{
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
	};
	for(auto& compositeAlphaFlags : compositeAlphaFlags)
	{
		if(surfaceCapabilities.supportedCompositeAlpha & compositeAlphaFlags)
		{
			compositeAlpha = compositeAlphaFlags;
			break;
		}
	}

	VkSwapchainCreateInfoKHR swapChainCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface,
		.minImageCount = desiredNumberOfSwapChainImages,
		.imageFormat = colorFormat,
		.imageColorSpace = colorSpace,
		.imageExtent = { swapChainExtent.width, swapChainExtent.height},
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode =VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform,
		.compositeAlpha = compositeAlpha,
		.presentMode = swapChainPresentMode,
		.clipped = VK_TRUE,
		.oldSwapchain = oldSwapChain
	};

	if(surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
	{
		swapChainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}
	if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
	{
		swapChainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}
	VK_CHECK_RESULT(vkCreateSwapchainKHR(device, &swapChainCreateInfo, nullptr, &swapChain));

	if(oldSwapChain != VK_NULL_HANDLE)
	{
		for(auto i = 0; i < images.size(); i++)
		{
			vkDestroyImageView(device, imageViews[i], nullptr);
		}
		vkDestroySwapchainKHR(device, oldSwapChain, nullptr);
	}

	VK_CHECK_RESULT(vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr));
	images.resize(imageCount);
	VK_CHECK_RESULT(vkGetSwapchainImagesKHR(device, swapChain, &imageCount, images.data()));

	imageViews.resize(imageCount);
	for(auto i = 0; i < images.size(); i++)
	{
		VkImageViewCreateInfo collorAttachemtnView
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = colorFormat,
			.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
			.subresourceRange =
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};
		VK_CHECK_RESULT(vkCreateImageView(device, &collorAttachemtnView, nullptr, &imageViews[i]));
	}
}


VkResult VulkanSwapChain::acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t& imageIndex)
{
	// with timeout being UINT64_MAX, we will always wait the next image or error, thus not needing to handle VK_NOT_READY
	return vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, presentCompleteSemaphore, (VkFence)nullptr, &imageIndex);
}

void VulkanSwapChain::cleanup()
{
	if(swapChain != VK_NULL_HANDLE)
	{
		for(auto i = 0; i < images.size(); i++)
		{
			vkDestroyImageView(device, imageViews[i], nullptr);
		}
		vkDestroySwapchainKHR(device, swapChain, nullptr);
	}

	if(surface != VK_NULL_HANDLE)
	{
		vkDestroySurfaceKHR(instance, surface, nullptr);
	}

	swapChain = VK_NULL_HANDLE;
	surface = VK_NULL_HANDLE;
}


