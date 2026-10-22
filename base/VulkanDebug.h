#ifndef VULKAN_DEBUG
#define VULKAN_DEBUG

#include <vulkan/vulkan.h>

#include <math.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <fstream>
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <sstream>
#include <Windows.h>
#include <fcntl.h>
#include <io.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace vkt
{
	namespace debug
	{
		extern bool logToFile;
		extern std::string logFilename;

		VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessaggeCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT messageType,
			const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
			void* pUserData
		);

		void setupDebugging(VkInstance instance);
		void freeDebugCallback(VkInstance instance);

		void setupDebuggingMessageCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& debugUtilsMessengerCI);
		void log(std::string message);
	}

	namespace debugutils
	{
		void setup(VkInstance instance);
		void cmdBeginLabel(VkCommandBuffer commandBuffer, std::string& caption, glm::vec4 color);
		void cmdEndLabel(VkCommandBuffer commandBuffer);
	}
}

#endif
