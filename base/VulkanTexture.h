#ifndef VULKAN_TEXTURE_H
#define VULKAN_TEXTURE_H

// texure loader for ktx files

#include <fstream>
#include <stdlib.h>
#include <string>
#include <vector>

#include "vulkan/vulkan.h"

#include <ktx.h>
#include <ktxvulkan.h>

#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "VulkanDebug.h"

namespace vkt
{
	class Texture
	{
	public:
		vkt::VulkanDevice* device;
		VkImage image;
		VkImageView view;
		VkImageLayout imageLayout;
		VkDeviceMemory deviceMemory;
		uint32_t width;
		uint32_t height;
		uint32_t mipLevels;
		uint32_t layerCount;
		VkDescriptorImageInfo descriptor;
		VkSampler sampler;
		VkFormat format;

		void updateDescriptor();
		void destroy();
		ktxResult loadKTXFile(std::string filename, ktxTexture** target);
	};

	class Texture2D : public Texture
	{
	public:
		void loadFromFile(
			std::string filename,
			VkFormat format,
			vkt::VulkanDevice* device,
			VkQueue copyQueue,
			VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
			VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);

		void fromBuffer(
			void* buffer,
			VkDeviceSize bufferSize,
			VkFormat format,
			uint32_t textureWidth,
			uint32_t textureHeight,
			vkt::VulkanDevice* device,
			VkQueue copyQueue,
			VkFilter filter = VK_FILTER_LINEAR,
			VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
			VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);
	};

	class Texture2DArray : Texture
	{
	public:
		void loadFromFile(
			std::string filename,
			VkFormat format,
			vkt::VulkanDevice* device,
			VkQueue copyQueue,
			VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
			VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);
	};

	class TextureCubeMap : Texture
	{
	public:
		void loadFromFile(
			std::string filename,
			VkFormat format,
			vkt::VulkanDevice* device,
			VkQueue copyQueue,
			VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
			VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);
	};
}

#endif

