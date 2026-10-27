#include "VulkanTexture.h"

namespace vkt
{
    void Texture::updateDescriptor()
    {
        descriptor.sampler = sampler;
        descriptor.imageLayout = imageLayout;
        descriptor.imageView = view;
    }

    void Texture::destroy()
    {
        vkDestroyImageView(device->logicalDevice, view, nullptr);
        vkDestroyImage(device->logicalDevice, image, nullptr);
        if(sampler)
        {
            vkDestroySampler(device->logicalDevice, sampler, nullptr);
        }
        vkFreeMemory(device->logicalDevice, deviceMemory, nullptr);
    }

    ktxResult Texture::loadKTXFile(std::string filename, ktxTexture** target)
    {
        ktxResult result = KTX_SUCCESS;
        if(!vkt::tools::fileExists(filename))
        {
            vkt::tools::exitFatal("could not load texture from " + filename + "\n\nMake sure the assets submodule has been checked and is up-to-date", -1);
        }
        result = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, target);
        return result;
        
    }

    /// load 2D texture with all mip levels
    void Texture2D::loadFromFile(std::string filename, VkFormat format, vkt::VulkanDevice* device, VkQueue copyQueue, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout)
    {
        ktxTexture* ktrTexture;
        ktxResult result = loadKTXFile(filename, &ktrTexture);
        assert(result == KTX_SUCCESS);
        
        this->device = device;
        width = ktrTexture->baseWidth;
        height = ktrTexture->baseHeight;
        mipLevels = ktrTexture->numLevels;
        this->format = format;

        ktx_uint8_t* ktxTextureData = ktxTexture_GetData(ktrTexture);
        ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktrTexture);

        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(device->physicalDevice, format, &formatProperties);

        // use a separete commanda buffer for texture loading
        VkCommandBuffer copyCommand = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        // host visible buffer that contains the raw image data
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        VkBufferCreateInfo bufferCreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = ktxTextureSize,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };
        VK_CHECK_RESULT(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

        // get mem requirements for staging buffer

        VkMemoryRequirements memoryReqs;
        vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memoryReqs);
        VkMemoryAllocateInfo memoryAllocateInfo
        {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memoryReqs.size,
            // memory type index for a host visible buffer
            .memoryTypeIndex = device->getMemoryType(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        };
        VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memoryAllocateInfo, nullptr, &stagingMemory));
        VK_CHECK_RESULT(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

        // copy texture data into staging buffer
        uint8_t* data{ nullptr };
        VK_CHECK_RESULT(vkMapMemory(device->logicalDevice, stagingMemory, 0, memoryReqs.size, 0, (void**)&data));
        memcpy(data, ktxTextureData, ktxTextureSize);
        vkUnmapMemory(device->logicalDevice, stagingMemory);

        // setup buffer copy regions for all mip levels
        std::vector<VkBufferImageCopy> bufferCopyRegions;
        for(uint32_t i = 0; i < mipLevels; i++)
        {
            ktx_size_t offset;
            KTX_error_code result = ktxTexture_GetImageOffset(ktrTexture, i, 0, 0, &offset);
            assert(result == KTX_SUCCESS);
            VkBufferImageCopy bufferCopyRegion
            {
                .bufferOffset = offset,
                .imageSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = i,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                },
                .imageExtent =
                {
                    .width = std::max(1u, ktrTexture->baseWidth >> i),
                    .height = std::max(1u, ktrTexture->baseHeight >> i),
                    .depth = 1
                }
            };
            bufferCopyRegions.push_back(bufferCopyRegion);
        }

        // create optimal tiled target image
        VkImageCreateInfo imageCreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = {.width = width, .height = height, .depth = 1 },
            .mipLevels = mipLevels,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = imageUsageFlags,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };

        if(!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        {
            imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }

        VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));
        vkGetImageMemoryRequirements(device->logicalDevice, image, &memoryReqs);
        memoryAllocateInfo.allocationSize = memoryReqs.size;
        memoryAllocateInfo.memoryTypeIndex = device->getMemoryType(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memoryAllocateInfo, nullptr, &deviceMemory));
        VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));

        VkImageSubresourceRange subResourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = mipLevels, .layerCount = 1 };

        // image barrier for optimal image target. optimal image will be used as destination for the copy
        vkt::tools::setImageLayout(
            copyCommand,
            image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subResourceRange
        );

        // copy mip levels from staging buffer
        vkCmdCopyBufferToImage(
            copyCommand,
            stagingBuffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(bufferCopyRegions.size()),
            bufferCopyRegions.data()
        );

        this->imageLayout = imageLayout;
        // change current image layout to shader read after copying all mip levels
        vkt::tools::setImageLayout(
            copyCommand,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            imageLayout,
            subResourceRange
        );

        device->flushCommandBuffer(copyCommand, copyQueue);
        
        // clean staging resources
        vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);
        vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);

        ktxTexture_Destroy(ktrTexture);
        
        
        // default sampler
        VkSamplerCreateInfo samplerCreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .mipLodBias = 0.0f,
            .anisotropyEnable = device->enabledFeatures.samplerAnisotropy,
            .maxAnisotropy = device->enabledFeatures.samplerAnisotropy ? device->properties.limits.maxSamplerAnisotropy : 1.0f,
            .compareOp = VK_COMPARE_OP_NEVER,
            .minLod = 0.0f,
            .maxLod = (float)mipLevels,
            .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
        };
        VK_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

        VkImageViewCreateInfo viewCreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = mipLevels, .baseArrayLayer = 0, .layerCount = 1}
        };
        VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCreateInfo, nullptr, &view));

        updateDescriptor();
    }

    void Texture2D::fromBuffer(void* buffer, VkDeviceSize bufferSize, VkFormat format, uint32_t textureWidth, uint32_t textureHeight, vkt::VulkanDevice* device, VkQueue copyQueue, VkFilter filter, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout)
    {
        assert(buffer);
        this->device = device;
        width = textureWidth;
        height = textureHeight;
        mipLevels = 1;

        VkMemoryAllocateInfo memoryAllocateInfo = vkt::initializers::memoryAllocateInfo();
        VkMemoryRequirements memoryReqs;

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        VkBufferCreateInfo bufferCreateInfo = vkt::initializers::bufferCreateInfo();
        bufferCreateInfo.size = bufferSize;
        bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK_RESULT(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

        vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memoryReqs);
        memoryAllocateInfo.allocationSize = memoryReqs.size;
        memoryAllocateInfo.memoryTypeIndex = device->getMemoryType(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);


        VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memoryAllocateInfo, nullptr, &stagingMemory));
        VK_CHECK_RESULT(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

        // copy texture data into staging buffer
        uint8_t* data{ nullptr };
        VK_CHECK_RESULT(vkMapMemory(device->logicalDevice, stagingMemory, 0, memoryReqs.size, 0, (void**)&data));
        memcpy(data, buffer, bufferSize);
        vkUnmapMemory(device->logicalDevice, stagingMemory);
        
        VkBufferImageCopy bufferCopyRegion
        {
            .bufferOffset = 0,
            .imageSubresource =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
            .imageExtent =
            {
                .width = width,
                .height = height,
                .depth = 1
            }
        };

        VkImageCreateInfo imageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = {.width = width, .height = height, .depth = 1 },
            .mipLevels = mipLevels,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = imageUsageFlags,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };

        if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT)) {
            imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }
        VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));

        vkGetImageMemoryRequirements(device->logicalDevice, image, &memoryReqs);

        memoryAllocateInfo.allocationSize = memoryReqs.size;

        memoryAllocateInfo.memoryTypeIndex = device->getMemoryType(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memoryAllocateInfo, nullptr, &deviceMemory));
        VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));

        VkImageSubresourceRange subResourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = mipLevels, .layerCount = 1 };

        VkCommandBuffer copyCommand = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        vkt::tools::setImageLayout(
            copyCommand,
            image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subResourceRange
        );

        // copy mip levels from staging buffer
        vkCmdCopyBufferToImage(
            copyCommand,
            stagingBuffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &bufferCopyRegion
        );
        this->imageLayout = imageLayout;

        vkt::tools::setImageLayout(
            copyCommand,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            imageLayout,
            subResourceRange
        );

        device->flushCommandBuffer(copyCommand, copyQueue);

        // clean staging resources
        vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);
        vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);


        // default sampler
        VkSamplerCreateInfo samplerCreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = filter,
            .minFilter = filter,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .mipLodBias = 0.0f,
            .maxAnisotropy = 1.0f,
            .compareOp = VK_COMPARE_OP_NEVER,
            .minLod = 0.0f,
            .maxLod = 0.0f,
        };
        VK_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

        VkImageViewCreateInfo viewCreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = mipLevels, .baseArrayLayer = 0, .layerCount = 1}
        };
        VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCreateInfo, nullptr, &view));

        updateDescriptor();
    }
    ///

    ///
    void Texture2DArray::loadFromFile(
        std::string filename,
        VkFormat format,
        vkt::VulkanDevice* device,
        VkQueue copyQueue,
        VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
        VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    )
    {
    
        ktxTexture* ktxTexture;
        ktxResult result = loadKTXFile(filename, &ktxTexture);
        assert(result == KTX_SUCCESS);

        this->device = device;
        width = ktxTexture->baseWidth;
        height = ktxTexture->baseHeight;
        mipLevels = ktxTexture->numLevels;
        layerCount = ktxTexture->numLayers;
        this->format = format;

        ktx_uint8_t* ktxTextureData = ktxTexture_GetData(ktxTexture);
        ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTexture);

        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(device->physicalDevice, format, &formatProperties);

        // use a separete commanda buffer for texture loading
        VkCommandBuffer copyCommand = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        // host visible buffer that contains the raw image data
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        VkBufferCreateInfo bufferCreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = ktxTextureSize,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };
        VK_CHECK_RESULT(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

        // get mem requirements for staging buffer

        VkMemoryRequirements memoryReqs;
        vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memoryReqs);
        VkMemoryAllocateInfo memoryAllocateInfo
        {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memoryReqs.size,
            // memory type index for a host visible buffer
            .memoryTypeIndex = device->getMemoryType(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        };
        VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memoryAllocateInfo, nullptr, &stagingMemory));
        VK_CHECK_RESULT(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

        // copy texture data into staging buffer
        uint8_t* data{ nullptr };
        VK_CHECK_RESULT(vkMapMemory(device->logicalDevice, stagingMemory, 0, memoryReqs.size, 0, (void**)&data));
        memcpy(data, ktxTextureData, ktxTextureSize);
        vkUnmapMemory(device->logicalDevice, stagingMemory);

        // setup buffer copy regions for all mip levels
        std::vector<VkBufferImageCopy> bufferCopyRegions;
        for(uint32_t layer = 0; layer < layerCount; layer++)
        {
            for (uint32_t level = 0; level < mipLevels; level++)
            {
                ktx_size_t offset;
                KTX_error_code result = ktxTexture_GetImageOffset(ktxTexture, level, layer, 0, &offset);
                assert(result == KTX_SUCCESS);
                VkBufferImageCopy bufferCopyRegion{
                    .bufferOffset = offset,
                    .imageSubresource {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .mipLevel = level,
                        .baseArrayLayer = layer,
                        .layerCount = 1,
                    },
                    .imageExtent {
                        .width = ktxTexture->baseWidth >> level,
                        .height = ktxTexture->baseHeight >> level,
                        .depth = 1,
                    }
                };
                bufferCopyRegions.push_back(bufferCopyRegion);
            }
        }

        VkImageCreateInfo imageCreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = {.width = width, .height = height, .depth = 1 },
            .mipLevels = mipLevels,
            .arrayLayers = layerCount,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = imageUsageFlags,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };

        if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        {
            imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }

        VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));
        vkGetImageMemoryRequirements(device->logicalDevice, image, &memoryReqs);
        memoryAllocateInfo.allocationSize = memoryReqs.size;
        memoryAllocateInfo.memoryTypeIndex = device->getMemoryType(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memoryAllocateInfo, nullptr, &deviceMemory));
        VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));

        VkImageSubresourceRange subResourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = mipLevels, .layerCount = layerCount };

        // image barrier for optimal image target. optimal image will be used as destination for the copy
        vkt::tools::setImageLayout(
            copyCommand,
            image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subResourceRange
        );

        // copy mip levels from staging buffer
        vkCmdCopyBufferToImage(
            copyCommand,
            stagingBuffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(bufferCopyRegions.size()),
            bufferCopyRegions.data()
        );

        this->imageLayout = imageLayout;
        // change current image layout to shader read after copying all mip levels
        vkt::tools::setImageLayout(
            copyCommand,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            imageLayout,
            subResourceRange
        );

        device->flushCommandBuffer(copyCommand, copyQueue);

        VkSamplerCreateInfo samplerCreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = samplerCreateInfo.addressModeU,
            .addressModeW = samplerCreateInfo.addressModeU,
            .mipLodBias = 0.0f,
            .anisotropyEnable = device->enabledFeatures.samplerAnisotropy,
            .maxAnisotropy = device->enabledFeatures.samplerAnisotropy ? device->properties.limits.maxSamplerAnisotropy : 1.0f,
            .compareOp = VK_COMPARE_OP_NEVER,
            .minLod = 0.0f,
            .maxLod = (float)mipLevels,
            .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
        };
        VK_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

        VkImageViewCreateInfo viewCreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
            .format = format,
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = mipLevels, .baseArrayLayer = 0, .layerCount = layerCount }
        };
        VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCreateInfo, nullptr, &view));

        ktxTexture_Destroy(ktxTexture);
        vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);
        vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);

        updateDescriptor();
    }
    ///

    ///
    void TextureCubeMap::loadFromFile(
        std::string filename,
        VkFormat format,
        vkt::VulkanDevice* device,
        VkQueue copyQueue,
        VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
        VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    )
    {
        ktxTexture* ktxTexture;
        ktxResult result = loadKTXFile(filename, &ktxTexture);
        assert(result == KTX_SUCCESS);

        this->device = device;
        width = ktxTexture->baseWidth;
        height = ktxTexture->baseHeight;
        mipLevels = ktxTexture->numLevels;
        this->format = format;

        ktx_uint8_t* ktxTextureData = ktxTexture_GetData(ktxTexture);
        ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTexture);

        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(device->physicalDevice, format, &formatProperties);

        // use a separete commanda buffer for texture loading
        VkCommandBuffer copyCommand = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        // host visible buffer that contains the raw image data
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        VkBufferCreateInfo bufferCreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = ktxTextureSize,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };
        VK_CHECK_RESULT(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

        // get mem requirements for staging buffer

        VkMemoryRequirements memoryReqs;
        vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memoryReqs);
        VkMemoryAllocateInfo memoryAllocateInfo
        {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memoryReqs.size,
            // memory type index for a host visible buffer
            .memoryTypeIndex = device->getMemoryType(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        };
        VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memoryAllocateInfo, nullptr, &stagingMemory));
        VK_CHECK_RESULT(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

        // copy texture data into staging buffer
        uint8_t* data{ nullptr };
        VK_CHECK_RESULT(vkMapMemory(device->logicalDevice, stagingMemory, 0, memoryReqs.size, 0, (void**)&data));
        memcpy(data, ktxTextureData, ktxTextureSize);
        vkUnmapMemory(device->logicalDevice, stagingMemory);

        // setup buffer copy regions for each face and all its mip levels
        std::vector<VkBufferImageCopy> bufferCopyRegions;
        for (uint32_t face = 0; face < 6; face++)
        {
            for (uint32_t level = 0; level < mipLevels; level++)
            {
                ktx_size_t offset;
                KTX_error_code result = ktxTexture_GetImageOffset(ktxTexture, level, face, 0, &offset);
                assert(result == KTX_SUCCESS);
                VkBufferImageCopy bufferCopyRegion{
                    .bufferOffset = offset,
                    .imageSubresource {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .mipLevel = level,
                        .baseArrayLayer = face,
                        .layerCount = 1,
                    },
                    .imageExtent {
                        .width = ktxTexture->baseWidth >> level,
                        .height = ktxTexture->baseHeight >> level,
                        .depth = 1,
                    }
                };
                bufferCopyRegions.push_back(bufferCopyRegion);
            }
        }

        VkImageCreateInfo imageCreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
            .imageType = VK_IMAGE_TYPE_2D,
            // required for cubmap
            .format = format,
            .extent = {.width = width, .height = height, .depth = 1 },
            .mipLevels = mipLevels,
            .arrayLayers = 6,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = imageUsageFlags,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };

        if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        {
            imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }

        VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));
        vkGetImageMemoryRequirements(device->logicalDevice, image, &memoryReqs);
        memoryAllocateInfo.allocationSize = memoryReqs.size;
        memoryAllocateInfo.memoryTypeIndex = device->getMemoryType(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memoryAllocateInfo, nullptr, &deviceMemory));
        VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));

        VkImageSubresourceRange subResourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = mipLevels, .layerCount = 6 };

        // image barrier for optimal image target. optimal image will be used as destination for the copy
        vkt::tools::setImageLayout(
            copyCommand,
            image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subResourceRange
        );

        // copy mip levels from staging buffer
        vkCmdCopyBufferToImage(
            copyCommand,
            stagingBuffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(bufferCopyRegions.size()),
            bufferCopyRegions.data()
        );

        this->imageLayout = imageLayout;
        // change current image layout to shader read after copying all mip levels
        vkt::tools::setImageLayout(
            copyCommand,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            imageLayout,
            subResourceRange
        );

        device->flushCommandBuffer(copyCommand, copyQueue);

        VkSamplerCreateInfo samplerCreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = samplerCreateInfo.addressModeU,
            .addressModeW = samplerCreateInfo.addressModeU,
            .mipLodBias = 0.0f,
            .anisotropyEnable = device->enabledFeatures.samplerAnisotropy,
            .maxAnisotropy = device->enabledFeatures.samplerAnisotropy ? device->properties.limits.maxSamplerAnisotropy : 1.0f,
            .compareOp = VK_COMPARE_OP_NEVER,
            .minLod = 0.0f,
            .maxLod = (float)mipLevels,
            .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
        };
        VK_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

        VkImageViewCreateInfo viewCreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_CUBE,
            .format = format,
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = mipLevels, .baseArrayLayer = 0, .layerCount = 6}
        };
        VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCreateInfo, nullptr, &view));

        ktxTexture_Destroy(ktxTexture);
        vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);
        vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);


        updateDescriptor();
    }
    ///
}