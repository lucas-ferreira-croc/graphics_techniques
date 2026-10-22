#include "VulkanDebug.h"

#include <iostream>

namespace vkt
{
	namespace debug
	{
		bool logToFile{ false };
		std::string logFilename{ "validation_output.txt" };

		PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
		PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
		VkDebugUtilsMessengerEXT debugUtilsMessenger;


		VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessaggeCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
		{
			std::string prefix;

			if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
			{
				prefix = "VERBOSE: ";

				if(!logToFile)
				{
					if(!logToFile)
					{
						prefix = "\033[32m" + prefix + "\033[0m";
					}
				}
			}
			else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
			{
				prefix = "INFO: ";
				if (!logToFile) {
					prefix = "\033[36m" + prefix + "\033[0m";
				}
			}
			else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
			{
				prefix = "WARNING: ";
				if (!logToFile) {
					prefix = "\033[33m" + prefix + "\033[0m";
				}
			}
			else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
			{
				prefix = "ERROR: ";
				if (!logToFile) {
					prefix = "\033[31m" + prefix + "\033[0m";
				}
			}


			std::stringstream debugMessage;
			if(pCallbackData->pMessageIdName)
			{
				debugMessage << prefix << "[" << pCallbackData->messageIdNumber << "][" << pCallbackData->pMessageIdName << "] : " << pCallbackData->pMessage;
			}
			else
			{ 
				debugMessage << prefix << "[" << pCallbackData->messageIdNumber << "] : " << pCallbackData->pMessage;
			}

			if(messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
			{
				std::cerr << debugMessage.str() << "\n\n";
			}
			else
			{
				std::cout << debugMessage.str() << "\n\n";
			}

			if(logToFile)
			{
				log(debugMessage.str());
			}
			fflush(stdout);

			return VK_FALSE;
		}

		void setupDebugging(VkInstance instance)
		{
			vkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
			vkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));

			VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo{};
			setupDebuggingMessageCreateInfo(debugUtilsMessengerCreateInfo);
			VkResult result = vkCreateDebugUtilsMessengerEXT(instance, &debugUtilsMessengerCreateInfo, nullptr, &debugUtilsMessenger);
			assert(result == VK_SUCCESS);
		}

		void freeDebugCallback(VkInstance instance)
		{
			if(debugUtilsMessenger != VK_NULL_HANDLE)
			{
				vkDestroyDebugUtilsMessengerEXT(instance, debugUtilsMessenger, nullptr);
			}
		}

		void setupDebuggingMessageCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& debugUtilsMessengerCI)
		{
			debugUtilsMessengerCI.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			debugUtilsMessengerCI.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			debugUtilsMessengerCI.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
			debugUtilsMessengerCI.pfnUserCallback = debugUtilsMessaggeCallback;
		}

		void log(std::string message)
		{
			if(logToFile)
			{
				time_t timestamp;
				time(&timestamp);
				std::ofstream logFile;
				logFile.open(logFilename, std::ios_base::app);
				logFile << strtok(ctime(&timestamp), "\n") << ": " << message << std::endl;
				logFile.close();
			}
		}
	}

	namespace debugutils 
	{
		PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT{ nullptr };
		PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT{ nullptr };
		PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT{ nullptr };

		void setup(VkInstance instance)
		{
			PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT"));
			PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT"));
			PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdInsertDebugUtilsLabelEXT>(vkGetInstanceProcAddr(instance, "vkCmdInsertDebugUtilsLabelEXT"));
		}
		void cmdBeginLabel(VkCommandBuffer commandBuffer, std::string& caption, glm::vec4 color)
		{
			if(!vkCmdBeginDebugUtilsLabelEXT)
			{
				return;
			}
			VkDebugUtilsLabelEXT labelInfo{};
			labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
			labelInfo.pLabelName = caption.c_str();
			memcpy(labelInfo.color, &color[0], sizeof(float) * 4);
			vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &labelInfo);
		}
		void cmdEndLabel(VkCommandBuffer commandBuffer)
		{
			if(!vkCmdEndDebugUtilsLabelEXT)
			{
				return;
			}
			vkCmdEndDebugUtilsLabelEXT(commandBuffer);
		}
	}
}