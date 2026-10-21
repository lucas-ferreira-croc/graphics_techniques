#include "VulkanTools.h"

const std::string getAssetPath()
{
	if(vkt::tools::resourcePath != "")
	{
		return vkt::tools::resourcePath + "/assets/";
	}

#if defined(VK_EXAMPLE_ASSETS_DIR)
	return VK_EXAMPLE_ASSETS_DIR;
#else
	return "../../assets/";
#endif
}

const std::string getShaderAsBasePath()
{
	if (vkt::tools::resourcePath != "")
	{
		return vkt::tools::resourcePath + "/shaders/";
	}

#if defined(VK_EXAMPLE_SHADERS_DIR)
	return VK_EXAMPLE_SHADERS_DIR;
#else
	return "../../shaders/";
#endif
}



namespace vkt
{
	namespace tools 
	{
		std::string resourcePath = "";
		bool errorModeSilnet = false;


		std::string errorString(VkResult errorCode)
		{
			switch (errorCode) 
			{
#define STR(r) case VK_ ##r: return #r
				STR(NOT_READY);
				STR(TIMEOUT);
				STR(EVENT_SET);
				STR(EVENT_RESET);
				STR(INCOMPLETE);
				STR(ERROR_OUT_OF_HOST_MEMORY);
				STR(ERROR_OUT_OF_DEVICE_MEMORY);
				STR(ERROR_INITIALIZATION_FAILED);
				STR(ERROR_DEVICE_LOST);
				STR(ERROR_MEMORY_MAP_FAILED);
				STR(ERROR_LAYER_NOT_PRESENT);
				STR(ERROR_EXTENSION_NOT_PRESENT);
				STR(ERROR_FEATURE_NOT_PRESENT);
				STR(ERROR_INCOMPATIBLE_DRIVER);
				STR(ERROR_TOO_MANY_OBJECTS);
				STR(ERROR_FORMAT_NOT_SUPPORTED);
				STR(ERROR_SURFACE_LOST_KHR);
				STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
				STR(SUBOPTIMAL_KHR);
				STR(ERROR_OUT_OF_DATE_KHR);
				STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
				STR(ERROR_VALIDATION_FAILED_EXT);
				STR(ERROR_INVALID_SHADER_NV);
				STR(ERROR_INCOMPATIBLE_SHADER_BINARY_EXT);
#undef STR
			default:
				return "UNKOWN_ERROR";
			}
		}

		std::string physicalDeviceTypeString(VkPhysicalDeviceType type)
		{
			switch (type)
			{
#define STR(r) case VK_PHYSICAL_DEVICE_TYPE_ ##r: return #r
				STR(OTHER);
				STR(INTEGRATED_GPU);
				STR(DISCRETE_GPU);
				STR(VIRTUAL_GPU);
				STR(CPU);
#undef STR(r)
			default:
				return "UNKOWN_DEVICE_TYPE";
			}
		}

		VkBool32 getSupportedDepthFormat(VkPhysicalDevice physicalDevice, VkFormat* depthFormat)
		{
			// depth format may be optinal, thefore we ought to find a suitable format to use
			// high to low precision
			std::vector<VkFormat> formatList =
			{
				VK_FORMAT_D32_SFLOAT_S8_UINT,
				VK_FORMAT_D32_SFLOAT,
				VK_FORMAT_D24_UNORM_S8_UINT,
				VK_FORMAT_D16_UNORM_S8_UINT,
				VK_FORMAT_D16_UNORM
			};

			for(auto& format : formatList)
			{
				VkFormatProperties formatProperties;
				vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);
				if(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
				{
					*depthFormat = format;
					return true;
				}
			}
			return false;
		}

		VkBool32 getSupportedDepthStencilFormat(VkPhysicalDevice physicalDevice, VkFormat* depthStencilFormat)
		{
			std::vector<VkFormat> formatList =
			{
				VK_FORMAT_D32_SFLOAT_S8_UINT,
				VK_FORMAT_D24_UNORM_S8_UINT,
				VK_FORMAT_D16_UNORM_S8_UINT
			};


			for (auto& format : formatList)
			{
				VkFormatProperties formatProperties;
				vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);
				if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
				{
					*depthStencilFormat = format;
					return true;
				}
			}
			return false;
		}
		// return if a given format supports LINEAR FILTERING
		VkBool32 formatIsFilterable(VkPhysicalDevice device, VkFormat format, VkImageTiling tiling)
		{
			VkFormatProperties formatProperties;
			vkGetPhysicalDeviceFormatProperties(device, format, &formatProperties);

			if (tiling == VK_IMAGE_TILING_OPTIMAL)
			{
				return formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
			}

			if (tiling == VK_IMAGE_TILING_LINEAR)
			{
				return formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
			}
			
			return false;
		}

		VkBool32 formatHasStencil(VkFormat format)
		{
			std::vector<VkFormat> stencilFormats =
			{
				VK_FORMAT_S8_UINT,
				VK_FORMAT_D16_UNORM_S8_UINT,
				VK_FORMAT_D24_UNORM_S8_UINT,
				VK_FORMAT_D32_SFLOAT_S8_UINT
			};

			return std::find(stencilFormats.begin(), stencilFormats.end(), format) != std::end(stencilFormats);
		}


		// craete an image barrier for changing the layout of an image and put it into an active command buffer
		void setImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkImageSubresourceRange subresourceRange, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
		{
			VkImageMemoryBarrier imageMemoryBarrier = vkt::initializers::imageMemoryBarrier();
			imageMemoryBarrier.oldLayout = oldImageLayout;
			imageMemoryBarrier.newLayout = newImageLayout;
			imageMemoryBarrier.image = image;
			imageMemoryBarrier.subresourceRange = subresourceRange;
			
			// source access mask cotrols actions that have to be finished in the old layout before transitioning to the new layout
			switch(oldImageLayout)
			{
			case VK_IMAGE_LAYOUT_UNDEFINED:
				// layout undefined or doest not - ONLY valid as initial layout
				imageMemoryBarrier.srcAccessMask = 0;
				break;
			case VK_IMAGE_LAYOUT_PREINITIALIZED:
				// image is preinitialized - ONLY valid as initial layout for LINEAR IMAGES, preserve memory contents
				// host writes needs to be finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
				break;
			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				// image is a color attachemt, writes to the color buffer need to be finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				break;
			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				// image is a depth/stencil attachemt, writes to the depth/stencil buffer need to be finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				break;
			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				// image is a transfer source, any reads from the image needs to be finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				break;
			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				// image is a transfer destination, any writes to the image needs to be finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				break;
			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				// image is read by a shader, any shader that reads tfrom the image have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
				break;
			default:
				break;
			}

			switch (newImageLayout)
			{
			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				// image is a transfer destination, any writes to the image needs to be finished
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				break;
			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				// image is a transfer source, any reads from the image needs to be finished
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				break;

			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				// image is a color attachemt, writes to the color buffer need to be finished
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				break;
			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				// image is a depth/stencil attachemt, writes to the depth/stencil buffer need to be finished
				imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				break;
			
			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				// image is read by a shader, any shader that reads tfrom the image have been finished
				if(imageMemoryBarrier.srcAccessMask == 0)
				{
					imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				}
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				break;
			default:
				break;
			}

			vkCmdPipelineBarrier(
				commandBuffer,
				srcStageMask,
				dstStageMask,
				0,
				0,
				nullptr,
				0,
				nullptr,
				1,
				&imageMemoryBarrier
			);
		}

		// fixed subresource on first mip level and layer
		void setImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkImageSubresourceRange subresourceRange, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
		{
			VkImageSubresourceRange subResourceRange = {};
			subResourceRange.aspectMask = aspectMask;
			subResourceRange.baseMipLevel = 0;
			subResourceRange.levelCount = 1;
			subResourceRange.layerCount = 1;
			setImageLayout(commandBuffer, image, oldImageLayout, newImageLayout, subResourceRange, srcStageMask, dstStageMask);
		}

		void insertImageMemoryBarrier(VkCommandBuffer commandBuffer, VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkImageSubresourceRange subResourceRange)
		{
			VkImageMemoryBarrier imageMemoryBarrier = vkt::initializers::imageMemoryBarrier();
			imageMemoryBarrier.srcAccessMask = srcAccessMask;
			imageMemoryBarrier.dstAccessMask = dstAccessMask;
			imageMemoryBarrier.oldLayout = oldImageLayout;
			imageMemoryBarrier.newLayout = newImageLayout;
			imageMemoryBarrier.subresourceRange = subResourceRange;
			imageMemoryBarrier.image = image;

			vkCmdPipelineBarrier(
				commandBuffer,
				srcStageMask,
				dstStageMask,
				0,
				0,
				nullptr,
				0,
				nullptr,
				1,
				&imageMemoryBarrier
			);
		}

		void exitFatal(const std::string& message, int32_t exitCode)
		{
			if(!errorModeSilent)
			{
				MessageBox(NULL, message.c_str(), NULL, MB_OK | MB_ICONERROR);
			}
			std::cerr << message << "\n";
			exit(exitCode);
		}

		void exitFatal(const std::string& message, VkResult resultCode)
		{
			exitFatal(message, (uint32_t)resultCode);
		}

		VkShaderModule loadShader(const char* filename, VkDevice device)
		{
			std::ifstream f(filename, std::ios::binary | std::ios::in | std::ios::ate);
			if (f.is_open())
			{
				size_t size = f.tellg();
				f.seekg(0, std::ios::beg);

				char* shaderCode = new char[size];
				f.read(shaderCode, size);
				f.close();

				assert(size > 0);

				VkShaderModule shaderModule;
				VkShaderModuleCreateInfo moduleCreateInfo{};
				moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
				moduleCreateInfo.codeSize = size;
				moduleCreateInfo.pCode = (uint32_t*)shaderCode;

				VK_CHECK_RESULT(vkCreateShaderModule(device, &moduleCreateInfo, NULL, &shaderModule));

				delete[] shaderCode;

				return shaderModule;
			}
			else
			{
				std::cerr << "Error: could not open sahder file " << filename << "\n";
				return VK_NULL_HANDLE;
			}
			
			return VkShaderModule();
		}

		bool fileExists(const std::string& filename)
		{
			std::ifstream f(filename.c_str());

			return !f.fail();
		}

		uint32_t alignedSize(uint32_t value, uint32_t alignment)
		{
			// alignment needs to be a value in power of 2
			// -1 is a safe margin
			// alignment -1 makes the number always have 4 bits set as 1
			// alignment 16 = 0001 000 and when we subtract 1, we get 15 which is equal to = 0000 1111
			// then we use the not sign to flip the bits, getting 1111 0000
			// this acts as mask, only allowing multiples of 16 (in this case, could be 32, 8, whatever power of 2 value)

			return (value + alignment - 1) & ~(alignment - 1);
		}

		size_t alignedSize(size_t value, size_t alignment)
		{
			return (value + alignment - 1) & ~(alignment - 1);
		}

		VkDeviceSize alignedVkSize(VkDeviceSize value, VkDeviceSize alignment)
		{
			return (value + alignment - 1) & ~(alignment - 1);
		}



	}
}

