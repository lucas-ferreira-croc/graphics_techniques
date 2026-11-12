#ifndef VULKANEXAMPLE_H	
#define VULKANEXAMPLE_H	

#ifdef _WIN32
#pragma comment(linker, "/subsystem:windows")
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <ShellScalingApi.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <array>
#include <unordered_map>
#include <numeric>
#include <ctime>
#include <iostream>
#include <chrono>
#include <random>
#include <algorithm>
#include <sys/stat.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>

#include "vulkan/vulkan.h"

#include "CommandLineParser.hpp"
#include "keycodes.hpp"
#include "VulkanTools.h"
#include "VulkanDebug.h"
#include "VulkanUIOverlay.h"
#include "VulkanSwapChain.h"
#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "VulkanTexture.h"

#include "VulkanInitializers.hpp"
#include "camera.hpp"

constexpr uint32_t maxConcurrentFrames{ 2 };

class VulkanExampleBase
{
public:
	VulkanExampleBase();
	virtual ~VulkanExampleBase();
	bool initVulkan();

#if defined(_WIN32)
	void setupConsole(std::string title);
	void setupDPIAwareness();
	HWND setupWindow(HINSTANCE hinstance, WNDPROC wndproc);
	void handleMessages(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif

	virtual VkResult createInstance();
	virtual void render() = 0;
	virtual void keyPressed(uint32_t);
	virtual void mouseMoved(double x, double y, bool& handled);
	virtual void windowResized();
	virtual void setupDepthStencil();
	virtual void setupFramebuffer();
	virtual void setupRenderPass();
	virtual void getEnabledFeatures();
	virtual void getEnabledExtensions();
	void beginDynamicRendering(VkCommandBuffer commandBuffer);
	void endDynamicRendering(VkCommandBuffer commandBuffer);

	virtual void prepare();

	VkPipelineShaderStageCreateInfo loadShader(std::string filename, VkShaderStageFlagBits stage);

	void windowResize();
	
	void renderLoop();

	void drawUI(const VkCommandBuffer commandBuffer);

	void prepareFrame(bool waitForFence = true);
	void submitFrame(bool skipQueueSubmit = true);

	virtual void OnUpdateUIOverlay(vkt::UIOverlay* overlay);

#if defined(_WIN32)
	virtual void OnHandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif

public:
	bool prepared = false;
	bool resized = false;
	uint32_t width = 1280 * 2;
	uint32_t height = 1280 * 2;

	vkt::UIOverlay ui;
	CommandLineParser commandLineParser;

	float frameTimer = 1.0f;

	vkt::VulkanDevice* vulkanDevice{};

	struct Settings 
	{
		bool validation = true;
		bool fullscreen = false;
		bool vsync = false;
		bool overlay = true;
	} settings;

	struct 
	{
		glm::vec2 axisLeft = glm::vec2(0.0f);
		glm::vec2 axisRight = glm::vec2(0.0f);
	} gamePadState;


	struct
	{
		struct
		{
			bool left = false;
			bool right = false;
			bool middle = false;
		} buttons;
		glm::vec2 position;
	} mouseState;

	VkClearColorValue defaultClearColor = { { 0.025f, 0.025f, 0.025f, 1.0f } };

	static std::vector<const char*> args;

	float timer = 0.0f;
	float timerSpeed = 0.25f;
	bool paused = false;

	Camera camera;

	std::string title = "vkt example";
	std::string name = "vktExample";
	uint32_t apiVersion = VK_API_VERSION_1_0;

	struct
	{
		VkImage image;
		VkDeviceMemory memory;
		VkImageView view;
	} depthStencil{};

	bool useDynamicRendering{ true };
	VkPhysicalDeviceDynamicRenderingFeatures baseDunamicRenderingFeatures
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
		.dynamicRendering = VK_TRUE
	};

#if defined(_WIN32)
	HWND window;
	HINSTANCE windowInstance;
#endif
protected:
	std::string getShadersPath() const;
protected:
	uint32_t frameCounter = 0;
	uint32_t lastFPS = 0;
	std::chrono::time_point<std::chrono::high_resolution_clock> lastTimeStamp;
	std::chrono::time_point<std::chrono::high_resolution_clock> tPreviousEnd;

	VkInstance instance{ VK_NULL_HANDLE };
	std::vector<std::string> supportedExtensions;

	VkPhysicalDevice physicalDevice{ VK_NULL_HANDLE };
	VkPhysicalDeviceProperties deviceProperties{};
	VkPhysicalDeviceFeatures deviceFeatures{};
	VkPhysicalDeviceMemoryProperties deviceMemoryProperties{};
	VkPhysicalDeviceFeatures enabledFeatures{};
	std::vector<const char*> enabledDeviceExtensions;
	std::vector<const char*> enabledInstanceExtensions;
	std::vector<VkLayerSettingEXT> enabledLayerSettings;
	void* deviceCreatepNextChain = nullptr;

	VkDevice device{ VK_NULL_HANDLE };
	VkQueue queue{ VK_NULL_HANDLE };
	VkFormat depthFormat{ VK_FORMAT_UNDEFINED };
	VkCommandPool commandPool{ VK_NULL_HANDLE };

	std::array<VkCommandBuffer, maxConcurrentFrames> drawCommandBuffers;
	VkRenderPass renderPass{ VK_NULL_HANDLE };
	std::vector<VkFramebuffer> frameBuffers;
	VkDescriptorPool descriptorPool{ VK_NULL_HANDLE };
	std::vector<VkShaderModule> shaderModules;
	VkPipelineCache pipelineCache{ VK_NULL_HANDLE };
	VulkanSwapChain swapChain;

	uint32_t currentImageIdex{ 0 };
	uint32_t currentBuffer{ 0 };
	std::array<VkSemaphore, maxConcurrentFrames> presentCompleteSemaphores{};
	std::vector<VkSemaphore> renderCompleteSemaphores{};
	std::array<VkFence, maxConcurrentFrames> waitFences;

	bool requiresStencil{ false };
private:
	std::string getWindowTitle() const;
	void handleMouseMove(uint32_t x, uint32_t y);
	void nextFrame();
	void updateOverlay();
	void createPipelineCache();
	void createCommandPool();
	void createSyncronizationPrimitives();
	void createSurface();
	void createCommandBuffers();
	void createSwapChain();
	void destroyCommandBuffers();

private:
	std::string shaderDir = "glsl";
	uint32_t destWidth{};
	uint32_t destHeight{};
	bool resizing = false;
};


#include "Entrypoints.h"

#endif
