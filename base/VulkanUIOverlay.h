// ui overlay class using imgui

#ifndef VULKAN_UI_OVERLAY_H
#define VULKAN_UI_OVERLAY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <sstream>
#include <iomanip>

#include <vulkan/vulkan.h>

#include "VulkanTools.h"
#include "VulkanDebug.h"
#include "VulkanBuffer.h"
#include "VulkanDevice.h"

#include "../external/imgui/imgui.h"

namespace vkt
{
	class UIOverlay
	{
	public:
		UIOverlay();
		~UIOverlay();

		void prepare();

		void upate(uint32_t currentBuffer);
		void draw(const VkCommandBuffer commandBuffer, uint32_t currentFrame);
		void resize(uint32_t width, uint32_t height);

		void freeResources();

		bool header(const char* caption);
		bool checkBox(const char* caption, bool* value);
		bool checkBox(const char* caption, int32_t* value);
		bool radioButton(const char* caption, bool value);
		bool inputFloat(const char* caption, float* value, float step, uint32_t precision);
		bool sliderFloat(const char* caption, float* value, float min, float max);
		bool sliderInt(const char* caption, int32_t* value, int32_t min, int32_t max);
		bool comboBox(const char* caption, int32_t* itemIndex, std::vector<std::string> items);
		bool button(const char* caption);
		bool colorPicker(const char* caption, float* color);
		void text(const char* formatstr, ...);
	public:
		vkt::VulkanDevice* device{ nullptr };
		VkQueue queue{ VK_NULL_HANDLE };

		VkSampleCountFlagBits rasterizationSamples{ VK_SAMPLE_COUNT_1_BIT };
		uint32_t subpass{ 0 };

		struct Buffers
		{
			vkt::Buffer vertexBuffer;
			vkt::Buffer indexBuffer;
			int32_t vertexCount{ 0 };
			int32_t indexCount{ 0 };
		};
		std::vector<Buffers> buffers;
		uint32_t maxConcurrentFrames{ 0 };
		uint32_t currentBuffer{ 0 };

		std::vector<VkPipelineShaderStageCreateInfo> shaders;

		VkDescriptorPool descriptorPool{ VK_NULL_HANDLE };
		VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };
		VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
		VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
		VkPipeline pipeline{ VK_NULL_HANDLE };

		VkDeviceMemory fontMemory{ VK_NULL_HANDLE };
		VkImage fontImage{ VK_NULL_HANDLE };
		VkImageView fontView{ VK_NULL_HANDLE };
		VkSampler sampler{ VK_NULL_HANDLE };

		struct PushConstantBlock
		{
			glm::vec2 scale;
			glm::vec2 translate;
		} pushConstantBlock;

		bool visible{ true };
		float scale{ 1.0f };

		VkRenderPass renderPass{ VK_NULL_HANDLE };
		VkFormat colorFormat{ VK_FORMAT_UNDEFINED };
		VkFormat depthFormat{ VK_FORMAT_UNDEFINED };
	};
}

#endif