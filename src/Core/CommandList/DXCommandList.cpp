#include "CommandList/DXCommandList.h"
#include <Device/DXDevice.h>
#include <Resource/DXResource.h>
#include <View/DXView.h>
#include <Utilities/DXUtility.h>
#include <Utilities/FileUtility.h>
#include <Pipeline/DXGraphicsPipeline.h>
#include <Pipeline/DXComputePipeline.h>
#include <Pipeline/DXRayTracingPipeline.h>
#include <RenderPass/DXRenderPass.h>
#include <Framebuffer/DXFramebuffer.h>
#include <BindingSet/DXBindingSet.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <gli/dx.hpp>
#include <pix.h>

DXCommandList::DXCommandList(DXDevice& device, CommandListType type)
    : m_device(device)
{
    m_use_render_passes = m_use_render_passes && m_device.IsRenderPassesSupported();

    D3D12_COMMAND_LIST_TYPE dx_type;
    switch (type)
    {
    case CommandListType::kGraphics:
        dx_type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        break;
    case CommandListType::kCompute:
        dx_type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        break;
    case CommandListType::kCopy:
        dx_type = D3D12_COMMAND_LIST_TYPE_COPY;
        break;
    default:
        assert(false);
        break;
    }
    ASSERT_SUCCEEDED(device.GetDevice()->CreateCommandAllocator(dx_type, IID_PPV_ARGS(&m_command_allocator)));
    ASSERT_SUCCEEDED(device.GetDevice()->CreateCommandList(0, dx_type, m_command_allocator.Get(), nullptr, IID_PPV_ARGS(&m_command_list)));

    m_command_list.As(&m_command_list4);
    m_command_list.As(&m_command_list5);
    m_command_list.As(&m_command_list6);
}

void DXCommandList::Reset()
{
    Close();
    ASSERT_SUCCEEDED(m_command_allocator->Reset());
    ASSERT_SUCCEEDED(m_command_list->Reset(m_command_allocator.Get(), nullptr));
    m_heaps.clear();
    m_state.reset();
    m_binding_set.reset();
    m_lazy_vertex.clear();
    m_closed = false;
}

void DXCommandList::Close()
{
    if (!m_closed)
    {
        m_command_list->Close();
        m_closed = true;
    }
}

void DXCommandList::BindPipeline(const std::shared_ptr<Pipeline>& state)
{
    if (state == m_state)
        return;
    auto type = state->GetPipelineType();
    if (type == PipelineType::kGraphics)
    {
        decltype(auto) dx_state = state->As<DXGraphicsPipeline>();
        m_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_command_list->SetGraphicsRootSignature(dx_state.GetRootSignature().Get());
        m_command_list->SetPipelineState(dx_state.GetPipeline().Get());
        for (const auto& x : dx_state.GetStrideMap())
        {
            auto it = m_lazy_vertex.find(x.first);
            if (it != m_lazy_vertex.end())
                IASetVertexBufferImpl(x.first, it->second, x.second);
            else
                IASetVertexBufferImpl(x.first, {}, 0);
        }
    }
    else if (type == PipelineType::kCompute)
    {
        decltype(auto) dx_state = state->As<DXComputePipeline>();
        m_command_list->SetComputeRootSignature(dx_state.GetRootSignature().Get());
        m_command_list->SetPipelineState(dx_state.GetPipeline().Get());
    }
    else if (type == PipelineType::kRayTracing)
    {
        decltype(auto) dx_state = state->As<DXRayTracingPipeline>();
        m_command_list->SetComputeRootSignature(dx_state.GetRootSignature().Get());
        m_command_list4->SetPipelineState1(dx_state.GetPipeline().Get());
    }
    m_state = state;
}

void DXCommandList::BindBindingSet(const std::shared_ptr<BindingSet>& binding_set)
{
    if (binding_set == m_binding_set)
        return;
    decltype(auto) dx_binding_set = binding_set->As<DXBindingSet>();
    decltype(auto) new_heaps = dx_binding_set.Apply(m_command_list);
    m_heaps.insert(m_heaps.end(), new_heaps.begin(), new_heaps.end());
    m_binding_set = binding_set;
}

void DXCommandList::BeginRenderPass(const std::shared_ptr<RenderPass>& render_pass, const std::shared_ptr<Framebuffer>& framebuffer, const ClearDesc& clear_desc)
{
    if (m_use_render_passes)
        BeginRenderPassImpl(render_pass, framebuffer, clear_desc);
    else
        OMSetFramebuffer(render_pass, framebuffer, clear_desc);
}

D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE Convert(RenderPassLoadOp op)
{
    switch (op)
    {
    case RenderPassLoadOp::kLoad:
        return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
    case RenderPassLoadOp::kClear:
        return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
    case RenderPassLoadOp::kDontCare:
        return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
    }
}

D3D12_RENDER_PASS_ENDING_ACCESS_TYPE Convert(RenderPassStoreOp op)
{
    switch (op)
    {
    case RenderPassStoreOp::kStore:
        return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
    case RenderPassStoreOp::kDontCare:
        return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
    }
}

void DXCommandList::BeginRenderPassImpl(const std::shared_ptr<RenderPass>& render_pass, const std::shared_ptr<Framebuffer>& framebuffer, const ClearDesc& clear_desc)
{
    decltype(auto) dx_render_pass = render_pass->As<DXRenderPass>();
    decltype(auto) dx_framebuffer = framebuffer->As<DXFramebuffer>();
    auto& rtvs = dx_framebuffer.GetRtvs();
    auto& dsv = dx_framebuffer.GetDsv();

    auto get_handle = [](const std::shared_ptr<View>& view)
    {
        if (!view)
            return D3D12_CPU_DESCRIPTOR_HANDLE{};
        decltype(auto) dx_view = view->As<DXView>();
        return dx_view.GetHandle();
    };

    std::vector<D3D12_RENDER_PASS_RENDER_TARGET_DESC> om_rtv;
    for (uint32_t slot = 0; slot < rtvs.size(); ++slot)
    {
        auto handle = get_handle(rtvs[slot]);
        if (!handle.ptr)
            continue;
        D3D12_RENDER_PASS_BEGINNING_ACCESS begin = { Convert(dx_render_pass.GetDesc().colors[slot].load_op), {} };
        if (slot < clear_desc.colors.size())
        {
            begin.Clear.ClearValue.Color[0] = clear_desc.colors[slot].r;
            begin.Clear.ClearValue.Color[1] = clear_desc.colors[slot].g;
            begin.Clear.ClearValue.Color[2] = clear_desc.colors[slot].b;
            begin.Clear.ClearValue.Color[3] = clear_desc.colors[slot].a;
        }
        D3D12_RENDER_PASS_ENDING_ACCESS end = { Convert(dx_render_pass.GetDesc().colors[slot].store_op), {} };
        om_rtv.push_back({ handle, begin, end });
    }

    D3D12_RENDER_PASS_DEPTH_STENCIL_DESC om_dsv = {};
    D3D12_RENDER_PASS_DEPTH_STENCIL_DESC* om_dsv_ptr = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE om_dsv_handle = get_handle(dsv);
    if (om_dsv_handle.ptr)
    {
        D3D12_RENDER_PASS_BEGINNING_ACCESS depth_begin = { Convert(dx_render_pass.GetDesc().depth_stencil.depth_load_op), {} };
        D3D12_RENDER_PASS_ENDING_ACCESS depth_end = { Convert(dx_render_pass.GetDesc().depth_stencil.depth_store_op), {} };
        D3D12_RENDER_PASS_BEGINNING_ACCESS stencil_begin = { Convert(dx_render_pass.GetDesc().depth_stencil.stencil_load_op), {} };
        D3D12_RENDER_PASS_ENDING_ACCESS stencil_end = { Convert(dx_render_pass.GetDesc().depth_stencil.stencil_store_op), {} };
        depth_begin.Clear.ClearValue.DepthStencil.Depth = clear_desc.depth;
        depth_begin.Clear.ClearValue.DepthStencil.Stencil = clear_desc.stencil;
        om_dsv = { om_dsv_handle, depth_begin, stencil_begin, depth_end, stencil_end };
        om_dsv_ptr = &om_dsv;
    }

    m_command_list4->BeginRenderPass(static_cast<uint32_t>(om_rtv.size()), om_rtv.data(), om_dsv_ptr, D3D12_RENDER_PASS_FLAG_NONE);
}

void DXCommandList::OMSetFramebuffer(const std::shared_ptr<RenderPass>& render_pass, const std::shared_ptr<Framebuffer>& framebuffer, const ClearDesc& clear_desc)
{
    decltype(auto) dx_render_pass = render_pass->As<DXRenderPass>();
    decltype(auto) dx_framebuffer = framebuffer->As<DXFramebuffer>();
    auto& rtvs = dx_framebuffer.GetRtvs();
    auto& dsv = dx_framebuffer.GetDsv();

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> om_rtv(rtvs.size());
    auto get_handle = [](const std::shared_ptr<View>& view)
    {
        if (!view)
            return D3D12_CPU_DESCRIPTOR_HANDLE{};
        decltype(auto) dx_view = view->As<DXView>();
        return dx_view.GetHandle();
    };
    for (uint32_t slot = 0; slot < rtvs.size(); ++slot)
    {
        om_rtv[slot] = get_handle(rtvs[slot]);
        if (dx_render_pass.GetDesc().colors[slot].load_op == RenderPassLoadOp::kClear)
            m_command_list->ClearRenderTargetView(om_rtv[slot], &clear_desc.colors[slot].x, 0, nullptr);
    }
    while (!om_rtv.empty() && om_rtv.back().ptr == 0)
    {
        om_rtv.pop_back();
    }
    D3D12_CPU_DESCRIPTOR_HANDLE om_dsv = get_handle(dsv);
    D3D12_CLEAR_FLAGS clear_flags = {};
    if (dx_render_pass.GetDesc().depth_stencil.depth_load_op == RenderPassLoadOp::kClear)
        clear_flags |= D3D12_CLEAR_FLAG_DEPTH;
    if (dx_render_pass.GetDesc().depth_stencil.stencil_load_op == RenderPassLoadOp::kClear)
        clear_flags |= D3D12_CLEAR_FLAG_STENCIL;
    if (om_dsv.ptr && clear_flags)
        m_command_list->ClearDepthStencilView(om_dsv, clear_flags, clear_desc.depth, clear_desc.stencil, 0, nullptr);
    m_command_list->OMSetRenderTargets(static_cast<uint32_t>(om_rtv.size()), om_rtv.data(), FALSE, om_dsv.ptr ? &om_dsv : nullptr);
}

void DXCommandList::EndRenderPass()
{
    if (!m_use_render_passes)
        return;
    m_command_list4->EndRenderPass();
}

void DXCommandList::BeginEvent(const std::string& name)
{
    if (!m_device.IsUnderGraphicsDebugger())
        return;
    std::wstring wname = utf8_to_wstring(name);
    PIXBeginEvent(m_command_list.Get(), 0, wname.c_str());
}

void DXCommandList::EndEvent()
{
    if (!m_device.IsUnderGraphicsDebugger())
        return;
    PIXEndEvent(m_command_list.Get());
}

void DXCommandList::DrawIndexed(uint32_t index_count, uint32_t start_index_location, int32_t base_vertex_location)
{
    m_command_list->DrawIndexedInstanced(index_count, 1, start_index_location, base_vertex_location, 0);
}

void DXCommandList::Dispatch(uint32_t thread_group_count_x, uint32_t thread_group_count_y, uint32_t thread_group_count_z)
{
    m_command_list->Dispatch(thread_group_count_x, thread_group_count_y, thread_group_count_z);
}

void DXCommandList::DispatchMesh(uint32_t thread_group_count_x)
{
    m_command_list6->DispatchMesh(thread_group_count_x, 1, 1);
}

void DXCommandList::DispatchRays(uint32_t width, uint32_t height, uint32_t depth)
{
    decltype(auto) dx_state = m_state->As<DXRayTracingPipeline>();
    D3D12_DISPATCH_RAYS_DESC raytrace_desc = dx_state.GetDispatchRaysDesc();
    raytrace_desc.Width = width;
    raytrace_desc.Height = height;
    raytrace_desc.Depth = depth;
    m_command_list4->DispatchRays(&raytrace_desc);
}

void DXCommandList::ResourceBarrier(const std::vector<ResourceBarrierDesc>& barriers)
{
    std::vector<D3D12_RESOURCE_BARRIER> dx_barriers;
    for (const auto& barrier : barriers)
    {
        if (!barrier.resource)
        {
            assert(false);
            continue;
        }
        if (barrier.state_before == ResourceState::kRaytracingAccelerationStructure)
            continue;

        decltype(auto) dx_resource = barrier.resource->As<DXResource>();
        D3D12_RESOURCE_STATES dx_state_before = ConvertState(barrier.state_before);
        D3D12_RESOURCE_STATES dx_state_after = ConvertState(barrier.state_after);
        if (dx_state_before == dx_state_after)
            continue;

        assert(barrier.base_mip_level + barrier.level_count <= dx_resource.desc.MipLevels);
        assert(barrier.base_array_layer + barrier.layer_count <= dx_resource.desc.DepthOrArraySize);

        if (barrier.base_mip_level == 0 && barrier.level_count == dx_resource.desc.MipLevels &&
            barrier.base_array_layer == 0 && barrier.layer_count == dx_resource.desc.DepthOrArraySize)
        {
            dx_barriers.emplace_back(CD3DX12_RESOURCE_BARRIER::Transition(dx_resource.resource.Get(), dx_state_before, dx_state_after));
        }
        else
        {
            for (uint32_t i = barrier.base_mip_level; i < barrier.base_mip_level + barrier.level_count; ++i)
            {
                for (uint32_t j = barrier.base_array_layer; j < barrier.base_array_layer + barrier.layer_count; ++j)
                {
                    uint32_t subresource = i + j * dx_resource.desc.MipLevels;
                    dx_barriers.emplace_back(CD3DX12_RESOURCE_BARRIER::Transition(dx_resource.resource.Get(), dx_state_before, dx_state_after, subresource));
                }
            }
        }
    }
    if (!dx_barriers.empty())
        m_command_list->ResourceBarrier(dx_barriers.size(), dx_barriers.data());
}

void DXCommandList::UAVResourceBarrier(const std::shared_ptr<Resource>& resource)
{
    D3D12_RESOURCE_BARRIER uav_barrier = {};
    uav_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    if (resource)
    {
        decltype(auto) dx_resource = resource->As<DXResource>();
        uav_barrier.UAV.pResource = dx_resource.resource.Get();
    }
    m_command_list4->ResourceBarrier(1, &uav_barrier);
}

void DXCommandList::SetViewport(float x, float y, float width, float height)
{
    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = x;
    viewport.TopLeftY = y;
    viewport.Width = width;
    viewport.Height = height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_command_list->RSSetViewports(1, &viewport);
}

void DXCommandList::SetScissorRect(int32_t left, int32_t top, uint32_t right, uint32_t bottom)
{
    D3D12_RECT rect = { left, top, right, bottom };
    m_command_list->RSSetScissorRects(1, &rect);
}

void DXCommandList::IASetIndexBuffer(const std::shared_ptr<Resource>& resource, gli::format format)
{
    DXGI_FORMAT dx_format = static_cast<DXGI_FORMAT>(gli::dx().translate(format).DXGIFormat.DDS);
    decltype(auto) dx_resource = resource->As<DXResource>();
    D3D12_INDEX_BUFFER_VIEW index_buffer_view = {};
    index_buffer_view.Format = dx_format;
    index_buffer_view.SizeInBytes = dx_resource.desc.Width;
    index_buffer_view.BufferLocation = dx_resource.resource->GetGPUVirtualAddress();
    m_command_list->IASetIndexBuffer(&index_buffer_view);
}

void DXCommandList::IASetVertexBuffer(uint32_t slot, const std::shared_ptr<Resource>& resource)
{
    if (m_state && m_state->GetPipelineType() == PipelineType::kGraphics)
    {
        decltype(auto) dx_state = m_state->As<DXGraphicsPipeline>();
        auto& strides = dx_state.GetStrideMap();
        auto it = strides.find(slot);
        if (it != strides.end())
            IASetVertexBufferImpl(slot, resource, it->second);
        else
            IASetVertexBufferImpl(slot, {}, 0);
    }
    m_lazy_vertex[slot] = resource;
}

void DXCommandList::IASetVertexBufferImpl(uint32_t slot, const std::shared_ptr<Resource>& resource, uint32_t stride)
{
    D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view = {};
    if (resource)
    {
        decltype(auto) dx_resource = resource->As<DXResource>();
        vertex_buffer_view.BufferLocation = dx_resource.resource->GetGPUVirtualAddress();
        vertex_buffer_view.SizeInBytes = dx_resource.desc.Width;
        vertex_buffer_view.StrideInBytes = stride;
    }
    m_command_list->IASetVertexBuffers(slot, 1, &vertex_buffer_view);
}

void DXCommandList::RSSetShadingRateImage(const std::shared_ptr<View>& view)
{
    std::shared_ptr<Resource> resource;
    if (view)
    {
        decltype(auto) dx_view = view->As<DXView>();
        resource = dx_view.GetResource();
    }

    if (resource)
    {
        decltype(auto) dx_resource = resource->As<DXResource>();
        const std::array<D3D12_SHADING_RATE_COMBINER, 2> combiners = {
            D3D12_SHADING_RATE_COMBINER_PASSTHROUGH, D3D12_SHADING_RATE_COMBINER_OVERRIDE
        };
        m_command_list5->RSSetShadingRate(D3D12_SHADING_RATE_1X1, combiners.data());
        m_command_list5->RSSetShadingRateImage(dx_resource.resource.Get());
    }
    else
    {
        m_command_list5->RSSetShadingRate(D3D12_SHADING_RATE_1X1, nullptr);
        m_command_list5->RSSetShadingRateImage(nullptr);
    }
}

void DXCommandList::BuildAccelerationStructure(D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs, const std::shared_ptr<Resource>& src, const std::shared_ptr<Resource>& dst, const std::shared_ptr<Resource>& scratch, uint64_t scratch_offset)
{
    decltype(auto) dx_dst = dst->As<DXResource>();
    decltype(auto) dx_scratch = scratch->As<DXResource>();

    inputs.Flags = dx_dst.as_flags;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC acceleration_structure_desc = {};
    acceleration_structure_desc.Inputs = inputs;
    if (src)
    {
        decltype(auto) dx_src = src->As<DXResource>();
        acceleration_structure_desc.SourceAccelerationStructureData = dx_src.resource->GetGPUVirtualAddress();
        acceleration_structure_desc.Inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
    }
    acceleration_structure_desc.DestAccelerationStructureData = dx_dst.resource->GetGPUVirtualAddress();
    acceleration_structure_desc.ScratchAccelerationStructureData = dx_scratch.resource->GetGPUVirtualAddress() + scratch_offset;
    m_command_list4->BuildRaytracingAccelerationStructure(&acceleration_structure_desc, 0, nullptr);
}

void DXCommandList::BuildBottomLevelAS(const std::shared_ptr<Resource>& src, const std::shared_ptr<Resource>& dst,
                                       const std::shared_ptr<Resource>& scratch, uint64_t scratch_offset, const std::vector<RaytracingGeometryDesc>& descs)
{
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometry_descs;
    for (const auto& desc : descs)
    {
        geometry_descs.emplace_back(FillRaytracingGeometryDesc(desc.vertex, desc.index, desc.flags));
    }
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.NumDescs = geometry_descs.size();
    inputs.pGeometryDescs = geometry_descs.data();
    BuildAccelerationStructure(inputs, src, dst, scratch, scratch_offset);
}

void DXCommandList::BuildTopLevelAS(const std::shared_ptr<Resource>& src, const std::shared_ptr<Resource>& dst, const std::shared_ptr<Resource>& scratch, uint64_t scratch_offset,
                                    const std::shared_ptr<Resource>& instance_data, uint64_t instance_offset, uint32_t instance_count)
{
    decltype(auto) dx_instance_data = instance_data->As<DXResource>();
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    inputs.NumDescs = instance_count;
    inputs.InstanceDescs = dx_instance_data.resource->GetGPUVirtualAddress() + instance_offset;
    BuildAccelerationStructure(inputs, src, dst, scratch, scratch_offset);
}

void DXCommandList::CopyAccelerationStructure(const std::shared_ptr<Resource>& src, const std::shared_ptr<Resource>& dst, CopyAccelerationStructureMode mode)
{
    decltype(auto) dx_src = src->As<DXResource>();
    decltype(auto) dx_dst = dst->As<DXResource>();
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE  dx_mode = {};
    switch (mode)
    {
    case CopyAccelerationStructureMode::kClone:
        dx_mode = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_CLONE;
        break;
    case CopyAccelerationStructureMode::kCompact:
        dx_mode = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_COMPACT;
        break;
    default:
        assert(false);
    }
    m_command_list4->CopyRaytracingAccelerationStructure(
        dx_dst.resource->GetGPUVirtualAddress(),
        dx_src.resource->GetGPUVirtualAddress(),
        dx_mode
    );
}

void DXCommandList::CopyBuffer(const std::shared_ptr<Resource>& src_buffer, const std::shared_ptr<Resource>& dst_buffer,
                               const std::vector<BufferCopyRegion>& regions)
{
    decltype(auto) dx_src_buffer = src_buffer->As<DXResource>();
    decltype(auto) dx_dst_buffer = dst_buffer->As<DXResource>();
    for (const auto& region : regions)
    {
        m_command_list->CopyBufferRegion(dx_dst_buffer.resource.Get(), region.dst_offset, dx_src_buffer.resource.Get(), region.src_offset, region.num_bytes);
    }
}

void DXCommandList::CopyBufferToTexture(const std::shared_ptr<Resource>& src_buffer, const std::shared_ptr<Resource>& dst_texture,
                                        const std::vector<BufferToTextureCopyRegion>& regions)
{
    decltype(auto) dx_src_buffer = src_buffer->As<DXResource>();
    decltype(auto) dx_dst_texture = dst_texture->As<DXResource>();
    auto format = dst_texture->GetFormat();
    DXGI_FORMAT dx_format = static_cast<DXGI_FORMAT>(gli::dx().translate(format).DXGIFormat.DDS);
    for (const auto& region : regions)
    {
        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource = dx_dst_texture.resource.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = region.texture_array_layer * dx_dst_texture.GetLevelCount() + region.texture_mip_level;

        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource = dx_src_buffer.resource.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint.Offset = region.buffer_offset;
        src.PlacedFootprint.Footprint.Width = region.texture_extent.width;
        src.PlacedFootprint.Footprint.Height = region.texture_extent.height;
        src.PlacedFootprint.Footprint.Depth = region.texture_extent.depth;
        if (gli::is_compressed(format))
        {
            auto extent = gli::block_extent(format);
            src.PlacedFootprint.Footprint.Width = std::max<uint32_t>(extent.x, src.PlacedFootprint.Footprint.Width);
            src.PlacedFootprint.Footprint.Height = std::max<uint32_t>(extent.y, src.PlacedFootprint.Footprint.Height);
            src.PlacedFootprint.Footprint.Depth = std::max<uint32_t>(extent.z, src.PlacedFootprint.Footprint.Depth);
        }
        src.PlacedFootprint.Footprint.RowPitch = region.buffer_row_pitch;
        src.PlacedFootprint.Footprint.Format = dx_format;

        m_command_list->CopyTextureRegion(&dst, region.texture_offset.x, region.texture_offset.y, region.texture_offset.z, &src, nullptr);
    }
}

void DXCommandList::CopyTexture(const std::shared_ptr<Resource>& src_texture, const std::shared_ptr<Resource>& dst_texture,
                                const std::vector<TextureCopyRegion>& regions)
{
    decltype(auto) dx_src_texture = src_texture->As<DXResource>();
    decltype(auto) dx_dst_texture = dst_texture->As<DXResource>();
    for (const auto& region : regions)
    {
        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource = dx_dst_texture.resource.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = region.dst_array_layer * dx_dst_texture.GetLevelCount() + region.dst_mip_level;

        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource = dx_src_texture.resource.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src.SubresourceIndex = region.src_array_layer * dx_src_texture.GetLevelCount() + region.src_mip_level;

        D3D12_BOX src_box = {};
        src_box.left = region.src_offset.x;
        src_box.top = region.src_offset.y;
        src_box.front = region.src_offset.z;
        src_box.right = region.src_offset.x + region.extent.width;
        src_box.bottom = region.src_offset.y + region.extent.height;
        src_box.back = region.src_offset.z + region.extent.depth;

        m_command_list->CopyTextureRegion(&dst, region.dst_offset.x, region.dst_offset.y, region.dst_offset.z, &src, &src_box);
    }
}

ComPtr<ID3D12GraphicsCommandList> DXCommandList::GetCommandList()
{
    return m_command_list;
}
