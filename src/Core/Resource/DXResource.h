#pragma once
#include "Resource/ResourceBase.h"
#include <d3d12.h>
#include <wrl.h>
using namespace Microsoft::WRL;

class DXDevice;

class DXResource : public ResourceBase
{
public:
    DXResource(DXDevice& device);

    void CommitMemory(MemoryType memory_type) override;
    void BindMemory(const std::shared_ptr<Memory>& memory, uint64_t offset) override;
    uint64_t GetWidth() const override;
    uint32_t GetHeight() const override;
    uint16_t GetLayerCount() const override;
    uint16_t GetLevelCount() const override;
    uint32_t GetSampleCount() const override;
    uint64_t GetAccelerationStructureHandle() const override;
    void SetName(const std::string& name) override;
    uint8_t* Map() override;
    void Unmap() override;
    bool AllowCommonStatePromotion(ResourceState state_after) override;
    MemoryRequirements GetMemoryRequirements() const override;

    ComPtr<ID3D12Resource> resource;
    D3D12_RESOURCE_DESC desc = {};
    D3D12_SAMPLER_DESC sampler_desc = {};
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS as_flags = {};

private:
    DXDevice& m_device;
};
