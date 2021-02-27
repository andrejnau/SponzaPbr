#pragma once
#include <map>
#include <algorithm>
#include <Utilities/Vulkan.h>
#include "GPUDescriptorPool/VKGPUDescriptorPoolRange.h"

constexpr uint32_t max_bindless_heap_size = 10000;

class VKDevice;

class VKGPUBindlessDescriptorPoolTyped
{
public:
    VKGPUBindlessDescriptorPoolTyped(VKDevice& device, vk::DescriptorType type);
    VKGPUDescriptorPoolRange Allocate(uint32_t count);
    void OnRangeDestroy(uint32_t offset, uint32_t size);
    vk::DescriptorSet GetDescriptorSet() const;

private:
    void ResizeHeap(uint32_t req_size);

    VKDevice& m_device;
    vk::DescriptorType m_type;
    uint32_t m_size = 0;
    uint32_t m_offset = 0;
    struct Descriptor
    {
        vk::UniqueDescriptorPool pool;
        vk::UniqueDescriptorSetLayout set_layout;
        vk::UniqueDescriptorSet set;
    } m_descriptor;
    std::multimap<uint32_t, uint32_t> m_empty_ranges;
};
