// tools and helper functions

#ifndef VULKAN_TOOLS_H
#define VULKAN_TOOLS_H

#include "vulkan/vulkan.h"
#include "VulkanInitializers.hpp"

#include <math.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <fstream>
#include <format>
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <algorithm>
#include <windows.h>
#include <fcntl.h>
#include <io.h>

#define VK_FLAGS_NONE 0

// fence timeout in nanoseconds
#define DEFAULT_FENCE_TIMEOUT 100000000000

#define VK_CHECK_RESULT(f)																											    \
{																																	    \
	VkResult result = (f);																											    \
	if(result != VK_SUCCESS)																										    \
	{																																    \
		std::string message = std::format("Fatal: VkResult is {} in {} at line {}", vkt::tools::errorString(result), __FILE__, __LINE__); \
		vkt::tools::exitFatal(message, -1);                                                                                             \
	}                                                                                                                                   \
}

const std::string getAssetPath();
const std::string getShaderAsBasePath();

namespace vkt 
{
	namespace tools
	{
		extern std::string resourcePath; // assets and shaders

		extern bool errorModeSilent;

		std::string errorString(VkResult errorCode);

		std::string physicalDeviceTypeString(VkPhysicalDeviceType type);

		// return false if none depth format in the list is supported by the devioce
		VkBool32 getSupportedDepthFormat(VkPhysicalDevice physicalDevice, VkFormat* depthFormat);

		// same as aboce but will only select formats that also have stencil
		VkBool32 getSupportedDepthStencilFormat(VkPhysicalDevice physicalDevice, VkFormat* depthStencilFormat);

		VkBool32 formatIsFilterable(VkPhysicalDevice device, VkFormat format, VkImageTiling tiling);

		VkBool32 formatHasStencil(VkFormat format);

		// put an image barrier for setting an image layout on the sub resource  into the given command buffer
		void setImageLayout(
			VkCommandBuffer commandBuffer,
			VkImage image,
			VkImageLayout oldImageLayout,
			VkImageLayout newImageLayout,
			VkImageSubresourceRange subresourceRange,
			VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
		);

		// uses a fixed sub resource layout with first mip level and layer
		void setImageLayout(
			VkCommandBuffer commandBuffer,
			VkImage image,
			VkImageAspectFlags aspectMask,
			VkImageLayout oldImageLayout,
			VkImageLayout newImageLayout,
			VkImageSubresourceRange subresourceRange,
			VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
		);

		// insert an image memory barrier into the command buffer
		void insertImageMemoryBarrier(
			VkCommandBuffer commandBuffer,
			VkImage image,
			VkAccessFlags srcAccessMask,
			VkAccessFlags dstAccessMask,
			VkImageLayout oldImageLayout,
			VkImageLayout newImageLayout,
			VkPipelineStageFlags srcStageMask,
			VkPipelineStageFlags dstStageMask,
			VkImageSubresourceRange subResourceRange
		);

		void exitFatal(const std::string& message, int32_t exitCode);
		void exitFatal(const std::string& message, VkResult resultCode);

		VkShaderModule loadShader(const char* filename, VkDevice device);

		bool fileExists(const std::string& filename);

		uint32_t alignedSize(uint32_t value, uint32_t alignment);
		size_t alignedSize(size_t value, size_t alignment);
		VkDeviceSize alignedVkSize(VkDeviceSize value, VkDeviceSize alignment);
	}
}

#endif