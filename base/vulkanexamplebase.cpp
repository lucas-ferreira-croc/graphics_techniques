#include "vulkanexamplebase.h"

std::vector<const char*> VulkanExampleBase::args;

VkResult VulkanExampleBase::createInstance()
{
    std::vector<const char*> instanceExtensions =
    {
        VK_KHR_SURFACE_EXTENSION_NAME
    };

#if defined(_WIN32)
    instanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif

    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    if (extensionCount > 0)
    {
        std::vector<VkExtensionProperties> extensions(extensionCount);
        if (vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, &extensions.front()) == VK_SUCCESS)
        {
            for (VkExtensionProperties& extension : extensions)
            {
                supportedExtensions.push_back(extension.extensionName);
            }
        }
    }

    if (!enabledInstanceExtensions.empty())
    {
        for (const char* enabledExtension : enabledInstanceExtensions)
        {
            if (std::find(supportedExtensions.begin(), supportedExtensions.end(), enabledExtension) == supportedExtensions.end())
            {
                std::cerr << "Enabled instance extension \"" << enabledExtension << "\" is not present at instance level\n";
            }
            instanceExtensions.push_back(enabledExtension);
        }
    }

    if (shaderDir == "slang")
    {
        if (apiVersion < VK_API_VERSION_1_1)
        {
            apiVersion = VK_API_VERSION_1_1;
        }
        enabledDeviceExtensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
        enabledDeviceExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
        enabledDeviceExtensions.push_back(VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME);
    }

    VkApplicationInfo appInfo
    {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = name.c_str(),
        .pEngineName = name.c_str(),
        .apiVersion = apiVersion
    };

    VkInstanceCreateInfo instanceCreateInfo
    {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo
    };

    VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI{};
    if (settings.validation)
    {
        vkt::debug::setupDebuggingMessageCreateInfo(debugUtilsMessengerCI);
        debugUtilsMessengerCI.pNext = instanceCreateInfo.pNext;
        instanceCreateInfo.pNext = &debugUtilsMessengerCI;
    }

    if (settings.validation || std::find(supportedExtensions.begin(), supportedExtensions.end(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != supportedExtensions.end())
    {
        instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    if (!instanceExtensions.empty())
    {
        instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
        instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
    }

    const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
    if (settings.validation)
    {
        uint32_t instanceLayerCount;
        vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
        std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
        vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());

        bool validationLayerPresent = false;
        for (VkLayerProperties& layer : instanceLayerProperties)
        {
            if (strcmp(layer.layerName, validationLayerName) == 0)
            {
                validationLayerPresent = true;
                break;
            }
        }
        if (validationLayerPresent)
        {
            instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
            instanceCreateInfo.enabledLayerCount = 1;
        }
        else
        {
            std::cerr << "validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled";
        }
    }

    VkLayerSettingsCreateInfoEXT layerSettingsCreateInfo
    {
        .sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT
    };

    if (enabledLayerSettings.size() > 0)
    {
        layerSettingsCreateInfo.settingCount = static_cast<uint32_t>(enabledLayerSettings.size());
        layerSettingsCreateInfo.pSettings = enabledLayerSettings.data();
        layerSettingsCreateInfo.pNext = instanceCreateInfo.pNext;
        instanceCreateInfo.pNext = &layerSettingsCreateInfo;
    }

    VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &instance);

    if (std::find(supportedExtensions.begin(), supportedExtensions.end(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != supportedExtensions.end())
    {
        vkt::debugutils::setup(instance);
    }

    return result;
}

void VulkanExampleBase::keyPressed(uint32_t)
{
}

void VulkanExampleBase::mouseMoved(double x, double y, bool& handled)
{
}

void VulkanExampleBase::windowResized()
{
}

void VulkanExampleBase::setupDepthStencil()
{
    VkImageCreateInfo imageCreateInfo
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = depthFormat,
        .extent = { .width = width, .height = height },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
    };
    VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &depthStencil.image));
    VkMemoryRequirements memoryReqs{};
    vkGetImageMemoryRequirements(device, depthStencil.image, &memoryReqs);

    VkMemoryAllocateInfo memoryAllocateInfo
    {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memoryReqs.size,
        .memoryTypeIndex = vulkanDevice->getMemoryType(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &depthStencil.memory));
    VK_CHECK_RESULT(vkBindImageMemory(device, depthStencil.image, depthStencil.memory, 0));

    VkImageViewCreateInfo imageViewCreateInfo
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = depthStencil.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = depthFormat,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    if(depthFormat >= VK_FORMAT_D16_UNORM_S8_UINT)
    {
        imageViewCreateInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &depthStencil.view));
}

void VulkanExampleBase::setupFramebuffer()
{
    if(useDynamicRendering)
    {
        renderPass = VK_NULL_HANDLE;
        return;
    }

    frameBuffers.resize(swapChain.images.size());
    for(uint32_t i = 0; i < frameBuffers.size(); i++)
    {
        const VkImageView attachemnts[2] = { swapChain.imageViews[i], depthStencil.view };
        VkFramebufferCreateInfo frameBufferCreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = renderPass,
            .attachmentCount = 2,
            .pAttachments = attachemnts,
            .width = width,
            .height = height,
            .layers = 1
        };
        VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCreateInfo, nullptr, &frameBuffers[i]));
    }
}

void VulkanExampleBase::setupRenderPass()
{
    if(useDynamicRendering)
    {
        renderPass = VK_NULL_HANDLE;
        return;
    }

    // dont know if I want to use render passes, probably not
}

void VulkanExampleBase::getEnabledFeatures()
{
}

void VulkanExampleBase::getEnabledExtensions()
{
}

void VulkanExampleBase::beginDynamicRendering(VkCommandBuffer commandBuffer)
{
    vkt::tools::insertImageMemoryBarrier(
        commandBuffer,
        swapChain.images[currentImageIdex],
        0,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    );

    vkt::tools::insertImageMemoryBarrier(
        commandBuffer,
        depthStencil.image,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 }
    );

    VkRenderingAttachmentInfoKHR colorAttachmentt
    {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = swapChain.imageViews[currentImageIdex],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = { .color = { 0.0f, 0.0f, 0.0f, 0.0f } }
    };

    VkRenderingAttachmentInfoKHR depthStencilAttachmentt
    {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = depthStencil.view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {.depthStencil = { 1.0f, 0 } }
    };

    VkRenderingInfoKHR renderingInfo
    {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
        .renderArea = { 0, 0, width, height },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentt,
        .pDepthAttachment = &depthStencilAttachmentt,
        .pStencilAttachment =&depthStencilAttachmentt
    };
    vkCmdBeginRendering(commandBuffer, &renderingInfo);
}

void VulkanExampleBase::endDynamicRendering(VkCommandBuffer commandBuffer)
{
    vkCmdEndRendering(commandBuffer);
    vkt::tools::insertImageMemoryBarrier(
        commandBuffer,
        swapChain.images[currentBuffer],
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        0,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    );
}

std::string VulkanExampleBase::getShadersPath() const
{
    return getShaderAsBasePath() + shaderDir + "/";
}

std::string VulkanExampleBase::getWindowTitle() const
{
    std::string windowTitle{ title + " - " + deviceProperties.deviceName };
    if(!settings.overlay)
    {
        windowTitle += " - " + std::to_string(frameCounter) + " fps";
    }
    return windowTitle;
}

void VulkanExampleBase::handleMouseMove(uint32_t x, uint32_t y)
{
    int32_t dx = (int32_t)mouseState.position.x - x;
    int32_t dy = (int32_t)mouseState.position.y - y;

    bool handled = false;

    if(settings.overlay)
    {
        ImGuiIO& io = ImGui::GetIO();
        handled = io.WantCaptureMouse && ui.visible;
    }
    mouseMoved((float)x, (float)y, handled);

    if(handled)
    {
        mouseState.position = glm::vec2((float)x, (float)y);
        return;
    }

    if(mouseState.buttons.left)
    {
        camera.rotate(glm::vec3(dy * camera.rotationSpeed, -dx * camera.rotationSpeed, 0.0f));
    }
    if(mouseState.buttons.right)
    {
        camera.translate(glm::vec3(-0.0f, 0.0f, dy * 0.005f));
    }
    if(mouseState.buttons.middle)
    {
        camera.translate(glm::vec3(-dx * 0.005f, -dy * 0.005f, 0.0f));
    }
    mouseState.position = glm::vec2((float)x, (float)y);
}

void VulkanExampleBase::nextFrame()
{
    auto tStart = std::chrono::high_resolution_clock::now();
    render();
    frameCounter++;
    auto tEnd = std::chrono::high_resolution_clock::now();

    auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

    frameTimer = (float)tDiff / 1000.0f;
    camera.update(frameTimer);
    
    if(!paused)
    {
        timer += timerSpeed * frameTimer;
        if(timer > 1.0f)
        {
            timer -= 1.0f;
        }
    }
    float fpsTimer = (float)(std::chrono::duration<double, std::milli>(tEnd - lastTimeStamp)).count();
    if(fpsTimer > 1000.0f)
    {
        lastFPS = static_cast<uint32_t>((float)frameCounter * (1000.0f / fpsTimer));
#if defined(_WIN32)
        if(!settings.overlay)
        {
            std::string windowTitle = getWindowTitle();
            SetWindowText(window, windowTitle.c_str());
        }
#endif
        frameCounter = 0;
        lastTimeStamp = tEnd;
    }
    tPreviousEnd = tEnd;
}

void VulkanExampleBase::updateOverlay()
{
    if(!settings.overlay)
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)width, (float)height);
    io.DeltaTime = frameTimer;
    io.MousePos = ImVec2(mouseState.position.x, mouseState.position.y);
    io.MouseDown[0] = mouseState.buttons.left && ui.visible;
    io.MouseDown[1] = mouseState.buttons.right && ui.visible;
    io.MouseDown[2] = mouseState.buttons.middle && ui.visible;

    ImGui::NewFrame();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::SetNextWindowPos(ImVec2(10 * ui.scale, 10 * ui.scale));
    ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiSetCond_FirstUseEver);
    ImGui::Begin("Vulkan example", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    ImGui::TextUnformatted(title.c_str());
    ImGui::TextUnformatted(deviceProperties.deviceName);
    ImGui::Text("Shading language: %s", shaderDir.c_str());
    ImGui::Text("%.2f ms/frame (%.1d fps)", (1000.0f) / lastFPS, lastFPS);
    ImGui::PushItemWidth(110.0f * ui.scale);
    OnUpdateUIOverlay(&ui);
    ImGui::PopItemWidth();
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::Render();

    ui.upate(currentBuffer);
}

void VulkanExampleBase::createCommandBuffers()
{
    VkCommandBufferAllocateInfo commandBufferAllocateInfo
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(drawCommandBuffers.size())
    };
    VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, drawCommandBuffers.data()));
}

void VulkanExampleBase::createSwapChain()
{
    swapChain.create(width, height, settings.vsync, settings.fullscreen);
}

void VulkanExampleBase::destroyCommandBuffers()
{
    vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(drawCommandBuffers.size()), drawCommandBuffers.data());
}

void VulkanExampleBase::createPipelineCache()
{
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO
    };
    VK_CHECK_RESULT(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
}

void VulkanExampleBase::createCommandPool()
{
    VkCommandPoolCreateInfo commandPoolInfo
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = swapChain.queueNodeIndex
    };
    VK_CHECK_RESULT(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool));
}

void VulkanExampleBase::createSyncronizationPrimitives()
{
    VkFenceCreateInfo fenceCreateInfo
    {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    for (auto& semaphore : presentCompleteSemaphores)
    {
        VkSemaphoreCreateInfo semaphoreCreateInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphore));
    }
    renderCompleteSemaphores.resize(swapChain.images.size());
    for(auto& semaphore : renderCompleteSemaphores)
    {
        VkSemaphoreCreateInfo semaphoreCreateInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphore));
    }
}

void VulkanExampleBase::createSurface()
{
#if defined(_WIN32)
    swapChain.initSurface(windowInstance, window);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
    swapChain.initSurface(androidApp->window);
#elif defined(VK_USE_PLATFORM_OHOS)
    swapChain.initSurface(nativeWindow);
#elif (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK))
    swapChain.initSurface(view);
#elif defined(VK_USE_PLATFORM_METAL_EXT)
    swapChain.initSurface(metalLayer);
#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
    swapChain.initSurface(dfb, surface);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    swapChain.initSurface(display, surface);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    swapChain.initSurface(connection, window);
#elif (defined(_DIRECT2DISPLAY) || defined(VK_USE_PLATFORM_HEADLESS_EXT))
    swapChain.initSurface(width, height);
#elif defined(VK_USE_PLATFORM_SCREEN_QNX)
    swapChain.initSurface(screen_context, screen_window);
#endif
}


void VulkanExampleBase::prepare()
{
    if(useDynamicRendering && apiVersion < VK_API_VERSION_1_3)
    {
        std::cerr << "Error: using the core variant of dynamic rendering requires vulkan 1.3";
        exit(-1);
    }

    createSurface();
    createCommandPool();
    createSwapChain();
    createCommandBuffers();
    createSyncronizationPrimitives();
    setupDepthStencil();
    setupRenderPass();
    createPipelineCache();
    setupFramebuffer();
    settings.overlay = settings.overlay; // && (!benchmark.active);
    if(settings.overlay)
    {
        ui.maxConcurrentFrames = maxConcurrentFrames;
        ui.device = vulkanDevice;
        ui.queue = queue;
        ui.renderPass = renderPass;
        ui.colorFormat = swapChain.colorFormat;
        ui.depthFormat = depthFormat;
        ui.shaders =
        {
                loadShader(getShaderAsBasePath() + "base/uioverlay.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
                loadShader(getShaderAsBasePath() + "base/uioverlay.frag.spv", VK_SHADER_STAGE_VERTEX_BIT)
        };
        ui.prepare();
    }
}

VkPipelineShaderStageCreateInfo VulkanExampleBase::loadShader(std::string filename, VkShaderStageFlagBits stage)
{

    VkPipelineShaderStageCreateInfo shaderStage
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = stage,
        .pName = "main"
    };

    shaderStage.module = vkt::tools::loadShader(filename.c_str(), device);

    assert(shaderStage.module != VK_NULL_HANDLE);
    shaderModules.push_back(shaderStage.module);
    return shaderStage;
}

void VulkanExampleBase::windowResize()
{
    if(!prepared)
    {
        return;
    }

    prepared = false;
    resized = true;

    vkDeviceWaitIdle(device);

    width = destWidth;
    height = destHeight;
    createSwapChain();

    vkDestroyImageView(device, depthStencil.view, nullptr);
    vkDestroyImage(device, depthStencil.image, nullptr);
    vkFreeMemory(device, depthStencil.memory, nullptr);

    setupDepthStencil();
    for(auto& frameBuffer : frameBuffers)
    {
        vkDestroyFramebuffer(device, frameBuffer, nullptr);
    }
    setupFramebuffer();

    if((width > 0.0f) && (height > 0.0f))
    {
        if(settings.overlay)
        {
            ui.resize(width, height);
        }
    }

    for(auto& semaphore : presentCompleteSemaphores)
    {
        vkDestroySemaphore(device, semaphore, nullptr);
    }

    for (auto& semaphore : renderCompleteSemaphores)
    {
        vkDestroySemaphore(device, semaphore, nullptr);
    }

    for(auto& fence : waitFences)
    {
        vkDestroyFence(device, fence, nullptr);
    }
    
    createSyncronizationPrimitives();

    if((width > 0.0f) && (height > 0.0f))
    {
        camera.updateAspectRatio((float)width / (float)height);
    }

    windowResized();
    prepared = true;
}

void VulkanExampleBase::renderLoop()
{
    destWidth = width;
    destHeight = height;
    lastTimeStamp = std::chrono::high_resolution_clock::now();
    tPreviousEnd = lastTimeStamp;

#if defined(_WIN32)
    MSG msg;
    bool quitMessageReceived = false;
    while(!quitMessageReceived)
    {
        while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if(msg.message = WM_QUIT)
            {
                quitMessageReceived = true;
                break;
            }
        }
        if(prepared && !IsIconic(window))
        {
            nextFrame();
        }
    }
#endif

    if(device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(device);
    }
}

void VulkanExampleBase::drawUI(const VkCommandBuffer commandBuffer)
{
    if(settings.overlay && ui.visible)
    {
        const VkViewport viewport
        {
            .width = (float)width,
            .height = (float)height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        const VkRect2D scissor
        {
            .extent =
            {
                .width = width,
                .height = height
            }
        };
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        ui.draw(commandBuffer, currentBuffer);
    }
}

void VulkanExampleBase::prepareFrame(bool waitForFence)
{
    if(waitForFence)
    {
        VK_CHECK_RESULT(vkWaitForFences(device, 1, &waitFences[currentBuffer], VK_TRUE, UINT64_MAX));
        VK_CHECK_RESULT(vkResetFences(device, 1, &waitFences[currentBuffer]));
    }
    updateOverlay();

    VkResult result = swapChain.acquireNextImage(presentCompleteSemaphores[currentBuffer], currentImageIdex);
    if((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR))
    {
        if(result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            windowResize();
        }
        return;
    }
    else
    {
        VK_CHECK_RESULT(result);
    }
}

void VulkanExampleBase::submitFrame(bool skipQueueSubmit)
{
    if(!skipQueueSubmit)
    {
        const VkPipelineStageFlags waitPipelineStage{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        VkSubmitInfo submitInfo
        {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &presentCompleteSemaphores[currentBuffer],
            .pWaitDstStageMask = &waitPipelineStage,
            .commandBufferCount = 1,
            .pCommandBuffers = &drawCommandBuffers[currentBuffer],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &renderCompleteSemaphores[currentBuffer]
        };
        VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, waitFences[currentBuffer]));
    }

    VkPresentInfoKHR presentInfo
    {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderCompleteSemaphores[currentBuffer],
        .swapchainCount = 1,
        .pSwapchains = &swapChain.swapChain,
        .pImageIndices = &currentImageIdex
    };
    VkResult result = vkQueuePresentKHR(queue, &presentInfo);
    if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR) || (result == VK_ERROR_SURFACE_LOST_KHR))
    {
        windowResize();
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            return;
        }
    }
    else
    {
        VK_CHECK_RESULT(result);
    }
    currentBuffer = (currentBuffer + 1) % maxConcurrentFrames;
}

void VulkanExampleBase::OnUpdateUIOverlay(vkt::UIOverlay* overlay)
{
}

VulkanExampleBase::VulkanExampleBase()
{
    commandLineParser.add("help", { "--help" }, 0, "Show help");
    commandLineParser.add("validation", { "-v", "--validation" }, 0, "Enable validation layers");
    commandLineParser.add("validationlogfile", { "-vl", "--validationlogfile" }, 0, "Log validation messages to a textfile");
    commandLineParser.add("vsync", { "-vs", "--vsync" }, 0, "Enable V-Sync");
    commandLineParser.add("fullscreen", { "-f", "--fullscreen" }, 0, "Start in fullscreen mode");
    commandLineParser.add("width", { "-w", "--width" }, 1, "Set window width");
    commandLineParser.add("height", { "-h", "--height" }, 1, "Set window height");
    commandLineParser.add("shaders", { "-s", "--shaders" }, 1, "Select shader type to use (gls, hlsl or slang)");
    commandLineParser.add("gpuselection", { "-g", "--gpu" }, 1, "Select GPU to run on");
    commandLineParser.add("gpulist", { "-gl", "--listgpus" }, 0, "Display a list of available Vulkan devices");

    commandLineParser.parse(args);
    if (commandLineParser.isSet("help")) {
#if defined(_WIN32)
        setupConsole("Vulkan example");
#endif
        commandLineParser.printHelp();
        std::cin.get();
        exit(0);
    }

    if (commandLineParser.isSet("validation")) {
        settings.validation = true;
    }
    if (commandLineParser.isSet("validationlogfile")) {
        vkt::debug::logToFile = true;
    }
    if (commandLineParser.isSet("vsync")) {
        settings.vsync = true;
    }
    if (commandLineParser.isSet("height")) {
        height = commandLineParser.getValueAsInt("height", height);
    }
    if (commandLineParser.isSet("width")) {
        width = commandLineParser.getValueAsInt("width", width);
    }
    if (commandLineParser.isSet("fullscreen")) {
        settings.fullscreen = true;
    }
    if (commandLineParser.isSet("shaders")) {
        std::string value = commandLineParser.getValueAsString("shaders", "glsl");
        if ((value != "glsl") && (value != "hlsl") && (value != "slang")) {
            std::cerr << "Shader type must be one of 'glsl', 'hlsl' or 'slang'\n";
        }
        else {
            shaderDir = value;
        }
    }

    struct stat info;
    if (stat(getAssetPath().c_str(), &info) != 0)
    {
#if defined(_WIN32)
        std::string msg = "Could not locate asset path in \"" + getAssetPath() + "\" !";
        MessageBox(NULL, msg.c_str(), "Fatal error", MB_OK | MB_ICONERROR);
#endif
        exit(-1);
    }

#if defined(FORCE_VALIDATION)
    settings.validation = true;
#endif

#if defined(_WIN32)
    // Enable console if validation is active, debug message callback will output to it
    if (this->settings.validation)
    {
        setupConsole("Vulkan example");
    }
    setupDPIAwareness();
#endif
}

VulkanExampleBase::~VulkanExampleBase()
{
    swapChain.cleanup();
    if(descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    }
    destroyCommandBuffers();
    if(renderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(device, renderPass, nullptr);
    }
    for(auto& frameBuffer : frameBuffers)
    {
        vkDestroyFramebuffer(device, frameBuffer, nullptr);
    }
    for(auto& shaderModule : shaderModules)
    {
        vkDestroyShaderModule(device, shaderModule, nullptr);
    }
    vkDestroyImageView(device, depthStencil.view, nullptr);
    vkDestroyImage(device, depthStencil.image, nullptr);
    vkFreeMemory(device, depthStencil.memory, nullptr);
    vkDestroyPipelineCache(device, pipelineCache, nullptr);
    vkDestroyCommandPool(device, commandPool, nullptr);
    
    for(auto& fence : waitFences)
    {
        vkDestroyFence(device, fence, nullptr);
    }

    for(auto& semaphore : presentCompleteSemaphores)
    {
        vkDestroySemaphore(device, semaphore, nullptr);
    }

    for (auto& semaphore : renderCompleteSemaphores)
    {
        vkDestroySemaphore(device, semaphore, nullptr);
    }

    if(settings.overlay)
    {
        ui.freeResources();
    }

    delete vulkanDevice;
    if(settings.validation)
    {
        vkt::debug::freeDebugCallback(instance);
    }
    vkDestroyInstance(instance, nullptr);

#if defined(_DIRECT2DISPLAY)

#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
    if (event_buffer)
        event_buffer->Release(event_buffer);
    if (surface)
        surface->Release(surface);
    if (window)
        window->Release(window);
    if (layer)
        layer->Release(layer);
    if (dfb)
        dfb->Release(dfb);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    xdg_toplevel_destroy(xdg_toplevel);
    xdg_surface_destroy(xdg_surface);
    wl_surface_destroy(surface);
    if (keyboard)
        wl_keyboard_destroy(keyboard);
    if (pointer)
        wl_pointer_destroy(pointer);
    if (seat)
        wl_seat_destroy(seat);
    xdg_wm_base_destroy(shell);
    wl_compositor_destroy(compositor);
    wl_registry_destroy(registry);
    wl_display_disconnect(display);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    xcb_destroy_window(connection, window);
    xcb_disconnect(connection);
#elif defined(VK_USE_PLATFORM_SCREEN_QNX)
    screen_destroy_event(screen_event);
    screen_destroy_window(screen_window);
    screen_destroy_context(screen_context);
#endif
}

bool VulkanExampleBase::initVulkan()
{
#if defined(_VALIDATION)
    this->settings.validation = true;
#endif

    if(commandLineParser.isSet("validationlogfile"))
    {
        vkt::debug::log("Sample: " + title);
    }

    VkResult result = createInstance();
    if(result != VK_SUCCESS)
    {
        vkt::tools::exitFatal("could not create vulkan instance: \n" + vkt::tools::errorString(result), result);
        return false;
    }

    if(settings.validation)
    {
        vkt::debug::setupDebugging(instance);
    }

    uint32_t gpuCount = 0;
    VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &gpuCount, &physicalDevice));
    if(gpuCount == 0)
    {
        vkt::tools::exitFatal("no device with vulkan support found", -1);
        return false;
    }

    std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
    result = vkEnumeratePhysicalDevices(instance, &gpuCount, physicalDevices.data());
    if(result != VK_SUCCESS)
    {
        vkt::tools::exitFatal("could not enumerate physical devices: \n" + vkt::tools::errorString(result), result);
        return false;
    }

    uint32_t selectedDevice = 0;

#if !defined(VK_USE_PLATFORM_ANDROID_KHR) && !defined(VK_USE_PLATFORM_OHOS)
    if(commandLineParser.isSet("gpuselection"))
    {
        uint32_t index = commandLineParser.getValueAsInt("gpuselection", 0);
        if(index > gpuCount - 1)
        {
            std::cerr << "selected device index " << index << " is out of range, reverting to device 0 (use -listgpus to show available vulkan devices)" << "\n";
        }
        else
        {
            selectedDevice = index;
        }
    }
    if(commandLineParser.isSet("gpulist"))
    {
        for(uint32_t i = 0; i < gpuCount; i++)
        {
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProperties);
            std::cout << "Device [" << i << "] : " << deviceProperties.deviceName << std::endl;
            std::cout << " Type: " << vkt::tools::physicalDeviceTypeString(deviceProperties.deviceType) << "\n";
            std::cout << " API: " << (deviceProperties.apiVersion >> 22) << "." << ((deviceProperties.apiVersion >> 12) & 0x3ff) << "." << (deviceProperties.apiVersion & 0xfff) << "\n";
        }
    }
#endif

    physicalDevice = physicalDevices[selectedDevice];

    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
    vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);

    getEnabledFeatures();

    vulkanDevice = new vkt::VulkanDevice(physicalDevice);

    getEnabledExtensions();

    result = vulkanDevice->createLogicalDevice(enabledFeatures, enabledDeviceExtensions, deviceCreatepNextChain);
    if(result != VK_SUCCESS)
    {
        vkt::tools::exitFatal("could not create vulkan device" + vkt::tools::errorString(result), result);
        return false;
    }
    device = vulkanDevice->logicalDevice;
    
    vkGetDeviceQueue(device, vulkanDevice->queueFamilyIndices.graphics, 0, &queue);

    VkBool32 validFormat{ false };

    if(requiresStencil)
    {
        validFormat = vkt::tools::getSupportedDepthStencilFormat(physicalDevice, &depthFormat);
    }
    else
    {
        validFormat = vkt::tools::getSupportedDepthFormat(physicalDevice, &depthFormat);
    }
    assert(validFormat);

    swapChain.setContext(instance, physicalDevice, device);
    return true;
}

#if defined(_WIN32)
void VulkanExampleBase::setupConsole(std::string title)
{
    AllocConsole();
    AttachConsole(GetCurrentProcessId());
    FILE* stream;
    freopen_s(&stream, "CONIN$", "r", stdin);
    freopen_s(&stream, "CONOUT$", "w+", stdout);
    freopen_s(&stream, "CONOUT$", "w+", stderr);

    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(consoleHandle, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(consoleHandle, dwMode);
    SetConsoleTitle(TEXT(title.c_str()));
}

void VulkanExampleBase::setupDPIAwareness()
{
    typedef HRESULT* (__stdcall* SetProcessDpiAwarenessFunc)(PROCESS_DPI_AWARENESS);

    HMODULE shCore = LoadLibraryA("Shcore.dll");
    if(shCore)
    {
        SetProcessDpiAwarenessFunc setProcessDpiAwareness = (SetProcessDpiAwarenessFunc)GetProcAddress(shCore, "SetProcessDpiAwareness");

        if(setProcessDpiAwareness != nullptr)
        {
            setProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
        }

        FreeLibrary(shCore);
    }
}

HWND VulkanExampleBase::setupWindow(HINSTANCE hinstance, WNDPROC wndproc)
{
    this->windowInstance = hinstance;
    WNDCLASSEX wndClass{
    .cbSize = sizeof(WNDCLASSEX),
    .style = CS_HREDRAW | CS_VREDRAW,
    .lpfnWndProc = wndproc,
    .cbClsExtra = 0,
    .cbWndExtra = 0,
    .hInstance = hinstance,
    .hIcon = LoadIcon(NULL, IDI_APPLICATION),
    .hCursor = LoadCursor(NULL, IDC_ARROW),
    .hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH),
    .lpszMenuName = NULL,
    .lpszClassName = name.c_str(),
    .hIconSm = LoadIcon(NULL, IDI_WINLOGO),
    };

    if(!RegisterClassExA(&wndClass))
    {
        std::cout << "could not register window class!\n";
        fflush(stdout);
        exit(-1);
    }

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    if(settings.fullscreen)
    {
        if((width != (uint32_t)screenWidth) && (height != (uint32_t)screenHeight))
        {
            DEVMODE dmScreenSettings;
            memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
            dmScreenSettings.dmSize = sizeof(dmScreenSettings);
            dmScreenSettings.dmPelsWidth = width;
            dmScreenSettings.dmPelsHeight = height;
            dmScreenSettings.dmBitsPerPel = 32;
            dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
            if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
            {
                if (MessageBox(NULL, "Fullscreen Mode not supported!\n Switch to window mode?", "Error", MB_YESNO | MB_ICONEXCLAMATION) == IDYES)
                {
                    settings.fullscreen = false;
                }
                else
                {
                    return nullptr;
                }
            }
            screenWidth = width;
            screenHeight = height;
        }
    }

    DWORD dwExStyle;
    DWORD dwStyle;

    if (settings.fullscreen)
    {
        dwExStyle = WS_EX_APPWINDOW;
        dwStyle = WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    }
    else
    {
        dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
        dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    }

    RECT windowRect{
        .left = 0L,
        .top = 0L,
        .right = settings.fullscreen ? (long)screenWidth : (long)width,
        .bottom = settings.fullscreen ? (long)screenHeight : (long)height
    };
    AdjustWindowRectEx(&windowRect, dwStyle, FALSE, dwExStyle);

    std::string windowTitle = getWindowTitle();
    window = CreateWindowEx(0,
        name.c_str(),
        windowTitle.c_str(),
        dwStyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        0,
        0,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        NULL,
        NULL,
        hinstance,
        NULL);

    if (!window)
    {
        std::cerr << "Could not create window!\n";
        fflush(stdout);
        return nullptr;
    }

    if (!settings.fullscreen)
    {
        // Center on screen
        uint32_t x = (GetSystemMetrics(SM_CXSCREEN) - windowRect.right) / 2;
        uint32_t y = (GetSystemMetrics(SM_CYSCREEN) - windowRect.bottom) / 2;
        SetWindowPos(window, 0, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
    }

    ShowWindow(window, SW_SHOW);
    SetForegroundWindow(window);

    return window;
}

void VulkanExampleBase::handleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CLOSE:
        prepared = false;
        DestroyWindow(hWnd);
        PostQuitMessage(0);
        break;
    case WM_PAINT:
        ValidateRect(window, NULL);
        break;
    case WM_KEYDOWN:
        switch (wParam)
        {
        case KEY_P:
            paused = !paused;
            break;
        case KEY_F1:
            ui.visible = !ui.visible;
            break;
        case KEY_F2:
            if (camera.type == Camera::CameraType::lookat) {
                camera.type = Camera::CameraType::firstperson;
            }
            else {
                camera.type = Camera::CameraType::lookat;
            }
            break;
        case KEY_ESCAPE:
            PostQuitMessage(0);
            break;
        }

        if (camera.type == Camera::firstperson)
        {
            switch (wParam)
            {
            case KEY_W:
                camera.keys.up = true;
                break;
            case KEY_S:
                camera.keys.down = true;
                break;
            case KEY_A:
                camera.keys.left = true;
                break;
            case KEY_D:
                camera.keys.right = true;
                break;
            }
        }

        keyPressed((uint32_t)wParam);
        break;
    case WM_KEYUP:
        if (camera.type == Camera::firstperson)
        {
            switch (wParam)
            {
            case KEY_W:
                camera.keys.up = false;
                break;
            case KEY_S:
                camera.keys.down = false;
                break;
            case KEY_A:
                camera.keys.left = false;
                break;
            case KEY_D:
                camera.keys.right = false;
                break;
            }
        }
        break;
    case WM_LBUTTONDOWN:
        mouseState.position = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
        mouseState.buttons.left = true;
        break;
    case WM_RBUTTONDOWN:
        mouseState.position = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
        mouseState.buttons.right = true;
        break;
    case WM_MBUTTONDOWN:
        mouseState.position = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
        mouseState.buttons.middle = true;
        break;
    case WM_LBUTTONUP:
        mouseState.buttons.left = false;
        break;
    case WM_RBUTTONUP:
        mouseState.buttons.right = false;
        break;
    case WM_MBUTTONUP:
        mouseState.buttons.middle = false;
        break;
    case WM_MOUSEWHEEL:
    {
        short wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        camera.translate(glm::vec3(0.0f, 0.0f, (float)wheelDelta * 0.005f));
        break;
    }
    case WM_MOUSEMOVE:
    {
        handleMouseMove(LOWORD(lParam), HIWORD(lParam));
        break;
    }
    case WM_SIZE:
        if ((prepared) && (wParam != SIZE_MINIMIZED))
        {
            if ((resizing) || ((wParam == SIZE_MAXIMIZED) || (wParam == SIZE_RESTORED)))
            {
                destWidth = LOWORD(lParam);
                destHeight = HIWORD(lParam);
                windowResize();
            }
        }
        break;
    case WM_GETMINMAXINFO:
    {
        LPMINMAXINFO minMaxInfo = (LPMINMAXINFO)lParam;
        minMaxInfo->ptMinTrackSize.x = 64;
        minMaxInfo->ptMinTrackSize.y = 64;
        break;
    }
    case WM_ENTERSIZEMOVE:
        resizing = true;
        break;
    case WM_EXITSIZEMOVE:
        resizing = false;
        break;
    }

    OnHandleMessage(hWnd, uMsg, wParam, lParam);
}

#if defined(_WIN32)
void VulkanExampleBase::OnHandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
}
#endif
#endif