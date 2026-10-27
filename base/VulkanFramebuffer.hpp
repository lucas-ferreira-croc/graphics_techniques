#ifndef VULKAN_FRAMEBUFFER_HPP
#define VULKAN_FRAMEBUFFER_HPP

// wrapper to vulkan framebuffer

#include <algorithm>
#include <iterator>
#include <vector>
#include <array>

#include "vulkan/vulkan.h"

#include "VulkanDevice.h"
#include "VulkanTools.h"

namespace vkt
{
	struct FramebufferAttachment
	{
		VkImage image;
		VkDeviceMemory memory;
		VkImageView view;
		VkFormat format;
		VkImageSubresourceRange subResourceRange;
		VkAttachmentDescription description;

		bool hasDepth()
		{
			std::vector<VkFormat> formats =
			{
				VK_FORMAT_D16_UNORM,
				VK_FORMAT_X8_D24_UNORM_PACK32,
				VK_FORMAT_D32_SFLOAT,
				VK_FORMAT_D16_UNORM_S8_UINT,
				VK_FORMAT_D24_UNORM_S8_UINT,
				VK_FORMAT_D32_SFLOAT_S8_UINT,
			};

			return std::find(formats.begin(), formats.end(), format) != std::end(formats);
		}

		bool hasStencil()
		{
			std::vector<VkFormat> formats =
			{
				VK_FORMAT_S8_UINT,
				VK_FORMAT_D16_UNORM_S8_UINT,
				VK_FORMAT_D24_UNORM_S8_UINT,
				VK_FORMAT_D32_SFLOAT_S8_UINT
			};
			return std::find(formats.begin(), formats.end(), format) != std::end(formats);
		}

		bool isDepthStencil()
		{
			return(hasDepth() || hasStencil());
		}
	};

	struct AttachemntCreateInfo
	{
		uint32_t width;
		uint32_t height;
		uint32_t layerCount;
		VkFormat format;
		VkImageUsageFlags usage;
		VkSampleCountFlagBits imageSampleCount = VK_SAMPLE_COUNT_1_BIT;
	};

	// arbitrary number and combination of attachments
	struct Framebuffer
	{
	public:
		uint32_t width;
		uint32_t height;
		VkFramebuffer framebuffer;
		VkRenderPass renderPass;
		VkSampler sampler;
		std::vector<vkt::FramebufferAttachment> attachments;

		Framebuffer(vkt::VulkanDevice* vulkanDevice)
		{
			assert(vulkanDevice);
			this->vulkanDevice = vulkanDevice;
		}

		~Framebuffer()
		{
			assert(vulkanDevice);
			for(auto attachment : attachments)
			{
				vkDestroyImage(vulkanDevice->logicalDevice, attachment.image, nullptr);
				vkDestroyImageView(vulkanDevice->logicalDevice, attachment.view, nullptr);
				vkFreeMemory(vulkanDevice->logicalDevice, attachment.memory, nullptr);
			}
			vkDestroySampler(vulkanDevice->logicalDevice, sampler, nullptr);
			vkDestroyRenderPass(vulkanDevice->logicalDevice, renderPass, nullptr);
			vkDestroyFramebuffer(vulkanDevice->logicalDevice, framebuffer, nullptr);
		}

		uint32_t addAttachment(vkt::AttachemntCreateInfo createInfo)
		{
			vkt::FramebufferAttachment attachment;
			attachment.format = createInfo.format;

			VkImageAspectFlags aspectMask = VK_FLAGS_NONE;

			if(createInfo.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
			{
				aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			}

			if(createInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
			{
				if(attachment.hasDepth())
				{
					aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
				}

				if (attachment.hasStencil())
				{
					aspectMask = aspectMask | VK_IMAGE_ASPECT_STENCIL_BIT;
				}
			}

			assert(aspectMask > 0);

			VkImageCreateInfo imageCreateInfo = vkt::initializers::imageCreateInfo();
			imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
			imageCreateInfo.format = createInfo.format;
			imageCreateInfo.extent.width = createInfo.width;
			imageCreateInfo.extent.height = createInfo.height;
			imageCreateInfo.extent.depth = 1;
			imageCreateInfo.mipLevels = createInfo.layerCount;
			imageCreateInfo.arrayLayers = createInfo.imageSampleCount;
			imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCreateInfo.usage = createInfo.usage;

			VkMemoryAllocateInfo memoryAllocateInfo = vkt::initializers::memoryAllocateInfo();
			VkMemoryRequirements memoryReqs;

			VK_CHECK_RESULT(vkCreateImage(vulkanDevice->logicalDevice, &imageCreateInfo, nullptr, &attachment.image));
			vkGetImageMemoryRequirements(vulkanDevice->logicalDevice, attachment.image, &memoryReqs);
			memoryAllocateInfo.allocationSize = memoryReqs.size;
			memoryAllocateInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(vulkanDevice->logicalDevice, &memoryAllocateInfo, nullptr, &attachment.memory));
			VK_CHECK_RESULT(vkBindImageMemory(vulkanDevice->logicalDevice, attachment.image, attachment.memory, 0));

			attachment.subResourceRange = {};
			attachment.subResourceRange.aspectMask = aspectMask;
			attachment.subResourceRange.levelCount = 1;
			attachment.subResourceRange.layerCount = createInfo.layerCount;

			VkImageViewCreateInfo imageViewCreateInfo = vkt::initializers::imageViewCreateInfo();
			imageViewCreateInfo.viewType = (createInfo.layerCount == 1) ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			imageViewCreateInfo.format = createInfo.format;
			imageViewCreateInfo.subresourceRange = attachment.subResourceRange;
			imageViewCreateInfo.subresourceRange.aspectMask = (attachment.hasDepth()) ? VK_IMAGE_ASPECT_DEPTH_BIT : aspectMask;
			imageViewCreateInfo.image = attachment.image;
			VK_CHECK_RESULT(vkCreateImageView(vulkanDevice->logicalDevice, &imageViewCreateInfo, nullptr, &attachment.view));

			attachment.description = {};
			attachment.description.samples = createInfo.imageSampleCount;
			attachment.description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachment.description.storeOp = (createInfo.usage & VK_IMAGE_USAGE_SAMPLED_BIT) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachment.description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachment.description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachment.description.format = createInfo.format;
			attachment.description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			if(attachment.hasDepth() || attachment.hasStencil())
			{
				attachment.description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			}
			else
			{
				attachment.description.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}

			attachments.push_back(attachment);
			
			return static_cast<uint32_t>(attachments.size() - 1);
		}

		VkResult createSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode addressMode)
		{
			VkSamplerCreateInfo samplerCreateInfo = vkt::initializers::samplerCreateInfo();
			samplerCreateInfo.magFilter = magFilter;
			samplerCreateInfo.minFilter = minFilter;
			samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerCreateInfo.addressModeU = addressMode;
			samplerCreateInfo.addressModeV = addressMode;
			samplerCreateInfo.addressModeW = addressMode;
			samplerCreateInfo.mipLodBias = 0.0f;
			samplerCreateInfo.maxAnisotropy = 1.0f;
			samplerCreateInfo.minLod = 0.0f;
			samplerCreateInfo.maxLod = 1.0f;
			samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			return vkCreateSampler(vulkanDevice->logicalDevice, &samplerCreateInfo, nullptr, &sampler);
		}

		VkResult createRenderePass()
		{
			std::vector<VkAttachmentDescription> attachmentDescriptions;
			for(auto& attachment : attachments)
			{
				attachmentDescriptions.push_back(attachment.description);
			}

			std::vector<VkAttachmentReference> colorReferences;
			VkAttachmentReference depthReference = {};
			bool hasDepth = false;
			bool hasColor = false;

			uint32_t attachmentIndex = 0;
			for(auto& attachment : attachments)
			{
				if(attachment.isDepthStencil())
				{
					// only one depth attachment allowed
					assert(!hasDepth);
					depthReference.attachment = attachmentIndex;
					depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					hasDepth = true;
				}
				else
				{
					colorReferences.push_back({	attachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
					hasColor = true;
				}
				attachmentIndex++;
			}

			// default render pass setup only uses one subpass
			VkSubpassDescription subpassDescription = {};
			subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			if(hasColor)
			{
				subpassDescription.pColorAttachments = colorReferences.data();
				subpassDescription.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
			}
			if(hasDepth)
			{
				subpassDescription.pDepthStencilAttachment = &depthReference;
			}

			// subpass dependencies for attachment layout transitions
			std::array<VkSubpassDependency, 4> dependencies{};

			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;


			dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].dstSubpass = 0;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[1].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			dependencies[2].srcSubpass = 0;
			dependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[2].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

			dependencies[3].srcSubpass = 0;
			dependencies[3].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[3].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dependencies[3].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependencies[3].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dependencies[3].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			VkRenderPassCreateInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassInfo.pAttachments = attachmentDescriptions.data();
			renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpassDescription;
			renderPassInfo.dependencyCount = 4;
			renderPassInfo.pDependencies = dependencies.data();
			VK_CHECK_RESULT(vkCreateRenderPass(vulkanDevice->logicalDevice, &renderPassInfo, nullptr, &renderPass));

			std::vector<VkImageView> attachmentViews;
			for(auto& attachemnt : attachments)
			{
				attachmentViews.push_back(attachemnt.view);
			}

			uint32_t maxLayers = 0;
			for (auto& attachemnt : attachments)
			{
				if(attachemnt.subResourceRange.layerCount > maxLayers)
				{
					maxLayers = attachemnt.subResourceRange.layerCount;
				}
			}

			VkFramebufferCreateInfo framebufferCreateInfo{};
			framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferCreateInfo.renderPass = renderPass;
			framebufferCreateInfo.pAttachments = attachmentViews.data();
			framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
			framebufferCreateInfo.width = width;
			framebufferCreateInfo.height = height;
			framebufferCreateInfo.layers = maxLayers;
			VK_CHECK_RESULT(vkCreateFramebuffer(vulkanDevice->logicalDevice, &framebufferCreateInfo, nullptr, &framebuffer));
			return VK_SUCCESS;
		}

	private:
		vkt::VulkanDevice* vulkanDevice;
	};
}

#endif