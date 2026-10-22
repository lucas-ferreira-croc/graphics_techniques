#include "VulkanBuffer.h"
// vulkan buffer

namespace vkt
{
    VkResult Buffer::map(VkDeviceSize size, VkDeviceSize offset)
    {
        return vkMapMemory(device, memory, offset, size, 0, &mapped);
    }

    void Buffer::unmap()
    {
        if(mapped)
        {
            vkUnmapMemory(device, memory);
            mapped = nullptr;
        }
    }

    VkResult Buffer::bind(VkDeviceSize offset)
    {
        return vkBindBufferMemory(device, buffer, memory, offset);
    }

    void Buffer::setupDescriptor(VkDeviceSize size, VkDeviceSize offset)
    {
        descriptor.offset = offset;
        descriptor.buffer = buffer;
        descriptor.range = size;
    }

    void Buffer::copyTo(void* data, VkDeviceSize size)
    {
        assert(mapped);
        memcpy(mapped, data, size);
    }

    // make it visible to deviec
    VkResult Buffer::flush(VkDeviceSize size, VkDeviceSize offset)
    {
        VkMappedMemoryRange mappedRange
        {
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .memory = memory,
            .offset = offset,
            .size = size,
        };

        return vkFlushMappedMemoryRanges(device, 1, &mappedRange);
    }
    
    // makes it visible to host
    VkResult Buffer::invalidate(VkDeviceSize, VkDeviceSize offset)
    {
        VkMappedMemoryRange mappedRange
        {
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .memory = memory,
            .offset = offset,
            .size = size,
        };

        return vkInvalidateMappedMemoryRanges(device, 1, &mappedRange);
    }

    void Buffer::destroy()
    {
        if(buffer)
        {
            vkDestroyBuffer(device, buffer, nullptr);
            buffer = VK_NULL_HANDLE;
        }
        
        if(memory)
        {
            vkFreeMemory(device, memory, nullptr);
            memory = VK_NULL_HANDLE;
        }
    }
}

