#include "RayTracingAOPass.h"

RayTracingAOPass::RayTracingAOPass(RenderDevice& device,
                                   RenderCommandList& command_list,
                                   const Input& input,
                                   int width,
                                   int height)
    : m_device(device)
    , m_input(input)
    , m_width(width)
    , m_height(height)
    , m_raytracing_program(device,
                           [&](auto& program) {
                               program.lib.desc.define["SAMPLE_COUNT"] =
                                   std::to_string(m_settings.Get<uint32_t>("sample_count"));
                           })
    , m_program_blur(device)
{
    CreateSizeDependentResources();
    m_sampler = m_device.CreateSampler({
        SamplerFilter::kAnisotropic,
        SamplerTextureAddressMode::kWrap,
        SamplerComparisonFunc::kNever,
    });

    std::vector<glm::uvec4> data;
    for (auto& model : m_input.scene_list) {
        for (auto& range : model.ia.ranges) {
            auto& material = model.GetMaterial(range.id);
            glm::uvec4 info = {};
            ViewDesc view_desc = {};
            view_desc.bindless = true;
            view_desc.dimension = ViewDimension::kTexture2D;
            view_desc.view_type = ViewType::kTexture;
            m_views.emplace_back(m_device.CreateView(material.texture.opacity, view_desc));
            info.x = m_views.back()->GetDescriptorId();

            view_desc.bindless = true;
            view_desc.dimension = ViewDimension::kBuffer;
            view_desc.view_type = ViewType::kStructuredBuffer;
            view_desc.offset = sizeof(uint32_t) * range.start_index_location;
            view_desc.structure_stride = sizeof(uint32_t);
            m_views.emplace_back(m_device.CreateView(model.ia.indices.GetBuffer(), view_desc));
            info.y = m_views.back()->GetDescriptorId();

            view_desc.bindless = true;
            view_desc.dimension = ViewDimension::kBuffer;
            view_desc.view_type = ViewType::kStructuredBuffer;
            view_desc.offset = sizeof(glm::vec2) * range.base_vertex_location;
            view_desc.structure_stride = sizeof(glm::vec2);
            m_views.emplace_back(m_device.CreateView(
                model.ia.texcoords.IsDynamic() ? model.ia.texcoords.GetDynamicBuffer() : model.ia.texcoords.GetBuffer(),
                view_desc));
            info.z = m_views.back()->GetDescriptorId();

            data.emplace_back(info);
        }
    }

    m_buffer = m_device.CreateBuffer(BindFlag::kShaderResource | BindFlag::kCopyDest, sizeof(glm::uvec4) * data.size());
    command_list.UpdateSubresource(m_buffer, 0, data.data());
}

void RayTracingAOPass::OnUpdate() {}

void RayTracingAOPass::OnRender(RenderCommandList& command_list)
{
    if (!m_settings.Get<bool>("use_rtao")) {
        return;
    }

    m_raytracing_program.lib.cbuffer.Settings.ao_radius = m_settings.Get<float>("ao_radius");
    m_raytracing_program.lib.cbuffer.Settings.num_rays = m_settings.Get<int32_t>("rtao_num_rays");
    m_raytracing_program.lib.cbuffer.Settings.use_alpha_test = m_settings.Get<bool>("use_alpha_test");

    auto build_geometry = [&](bool force_rebuild) {
        size_t id = 0;
        size_t node_updated = 0;
        bool has_dynamic_geometry = false;
        for (auto& model : m_input.scene_list) {
            for (auto& range : model.ia.ranges) {
                auto& material = model.GetMaterial(range.id);
                RaytracingGeometryFlags flags = RaytracingGeometryFlags::kOpaque;
                if (material.texture.opacity->GetWidth() != 1 || material.texture.opacity->GetHeight() != 1) {
                    flags = RaytracingGeometryFlags::kNone;
                }

                size_t cur_id = id++;
                if (!force_rebuild && !model.ia.positions.IsDynamic()) {
                    continue;
                }

                BufferDesc vertex = {
                    model.ia.positions.IsDynamic() ? model.ia.positions.GetDynamicBuffer()
                                                   : model.ia.positions.GetBuffer(),
                    gli::format::FORMAT_RGB32_SFLOAT_PACK32,
                    (uint32_t)model.ia.positions.Count() - (uint32_t)range.base_vertex_location,
                    (uint32_t)range.base_vertex_location,
                };

                BufferDesc index = {
                    model.ia.indices.GetBuffer(),
                    model.ia.indices.Format(),
                    range.index_count,
                    range.start_index_location,
                };

                if (cur_id >= m_bottom.size()) {
                    m_bottom.resize(cur_id + 1);
                }

                if (cur_id >= m_geometry.size()) {
                    m_geometry.resize(cur_id + 1);
                }

                auto& dst = m_bottom[cur_id];

                std::shared_ptr<Resource> src;
                BuildAccelerationStructureFlags build_flags = BuildAccelerationStructureFlags::kPreferFastTrace;
                if (model.ia.positions.IsDynamic()) {
                    has_dynamic_geometry = true;
                    src = dst;
                    build_flags = BuildAccelerationStructureFlags::kAllowUpdate |
                                  BuildAccelerationStructureFlags::kPreferFastBuild;
                }

                std::vector<RaytracingGeometryDesc> descs = { { vertex, index, flags } };

                if (!dst) {
                    dst = m_device.CreateBottomLevelAS(descs, build_flags);
                    command_list.BuildBottomLevelAS({}, dst, descs, build_flags);
                } else if (src && dst) {
                    command_list.BuildBottomLevelAS(src, dst, descs, build_flags);
                }

                m_geometry[cur_id] = { dst, glm::transpose(model.matrix) };
                ++node_updated;
            }
        }
        if (node_updated) {
            BuildAccelerationStructureFlags build_flag = BuildAccelerationStructureFlags::kNone;
            std::shared_ptr<Resource> src;
            if (has_dynamic_geometry) {
                build_flag = BuildAccelerationStructureFlags::kAllowUpdate;
                src = m_top;
            }

            if (!m_top) {
                m_top = m_device.CreateTopLevelAS(m_geometry.size(), build_flag);
                command_list.BuildTopLevelAS(src, m_top, m_geometry, build_flag);
            } else if (src && m_top) {
                command_list.BuildTopLevelAS(src, m_top, m_geometry, build_flag);
            }
        }
    };

    build_geometry(!m_is_initialized);

    if (!m_is_initialized) {
        m_raytracing_program.lib.cbuffer.Settings.frame_index = 0;
    } else {
        ++m_raytracing_program.lib.cbuffer.Settings.frame_index;
    }
    m_is_initialized = true;

    command_list.UseProgram(m_raytracing_program);
    command_list.Attach(m_raytracing_program.lib.cbv.Settings, m_raytracing_program.lib.cbuffer.Settings);
    command_list.Attach(m_raytracing_program.lib.srv.gPosition, m_input.geometry_pass.position);
    command_list.Attach(m_raytracing_program.lib.srv.gNormal, m_input.geometry_pass.normal);
    command_list.Attach(m_raytracing_program.lib.srv.geometry, m_top);
    command_list.Attach(m_raytracing_program.lib.uav.result, m_ao);
    command_list.Attach(m_raytracing_program.lib.sampler.gSampler, m_sampler);
    command_list.Attach(m_raytracing_program.lib.srv.descriptor_offset, m_buffer);
    command_list.DispatchRays(m_width, m_height, 1);

    if (m_settings.Get<bool>("use_ao_blur")) {
        command_list.SetViewport(0, 0, m_width, m_height);
        command_list.UseProgram(m_program_blur);
        command_list.Attach(m_program_blur.ps.uav.out_uav, m_ao_blur);
        command_list.Attach(m_program_blur.ps.sampler.g_sampler, m_sampler);

        m_input.square.ia.indices.Bind(command_list);
        m_input.square.ia.positions.BindToSlot(command_list, m_program_blur.vs.ia.POSITION);
        m_input.square.ia.texcoords.BindToSlot(command_list, m_program_blur.vs.ia.TEXCOORD);

        command_list.BeginRenderPass({});
        for (auto& range : m_input.square.ia.ranges) {
            command_list.Attach(m_program_blur.ps.srv.ssaoInput, m_ao);
            command_list.DrawIndexed(range.index_count, 1, range.start_index_location, range.base_vertex_location, 0);
        }
        command_list.EndRenderPass();

        output.ao = m_ao_blur;
    } else {
        output.ao = m_ao;
    }
}

void RayTracingAOPass::OnResize(int width, int height)
{
    m_width = width;
    m_height = height;
    CreateSizeDependentResources();
}

void RayTracingAOPass::CreateSizeDependentResources()
{
    m_ao = m_device.CreateTexture(BindFlag::kShaderResource | BindFlag::kUnorderedAccess,
                                  gli::format::FORMAT_RGBA32_SFLOAT_PACK32, 1, m_width, m_height, 1);
    m_ao_blur = m_device.CreateTexture(BindFlag::kShaderResource | BindFlag::kUnorderedAccess,
                                       gli::format::FORMAT_RGBA32_SFLOAT_PACK32, 1, m_width, m_height, 1);
}

void RayTracingAOPass::OnModifySponzaSettings(const SponzaSettings& settings)
{
    SponzaSettings prev = m_settings;
    m_settings = settings;
    if (prev.Get<uint32_t>("sample_count") != m_settings.Get<uint32_t>("sample_count")) {
        m_raytracing_program.lib.desc.define["SAMPLE_COUNT"] = std::to_string(m_settings.Get<uint32_t>("sample_count"));
        m_raytracing_program.UpdateProgram();
    }
}
