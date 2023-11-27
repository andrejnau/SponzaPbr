#include "ShadowPass.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

ShadowPass::ShadowPass(RenderDevice& device, const Input& input)
    : m_device(device)
    , m_input(input)
    , m_program(device)
{
    CreateSizeDependentResources();
    m_sampler = m_device.CreateSampler({
        SamplerFilter::kAnisotropic,
        SamplerTextureAddressMode::kWrap,
        SamplerComparisonFunc::kNever,
    });
}

void ShadowPass::OnUpdate()
{
    glm::vec3 Up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 Down = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 Left = glm::vec3(-1.0f, 0.0f, 0.0f);
    glm::vec3 Right = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 ForwardRH = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 ForwardLH = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 BackwardRH = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 BackwardLH = glm::vec3(0.0f, 0.0f, -1.0f);

    glm::vec3 position = m_input.light_pos;

    m_program.vs.cbuffer.VSParams.Projection = glm::transpose(
        glm::perspective(glm::radians(90.0f), 1.0f, m_settings.Get<float>("s_near"), m_settings.Get<float>("s_far")));

    std::array<glm::mat4, 6>& view = m_program.vs.cbuffer.VSParams.View;
    view[0] = glm::transpose(glm::lookAt(position, position + Right, Up));
    view[1] = glm::transpose(glm::lookAt(position, position + Left, Up));
    view[2] = glm::transpose(glm::lookAt(position, position + Up, BackwardRH));
    view[3] = glm::transpose(glm::lookAt(position, position + Down, ForwardRH));
    view[4] = glm::transpose(glm::lookAt(position, position + BackwardLH, Up));
    view[5] = glm::transpose(glm::lookAt(position, position + ForwardLH, Up));
}

void ShadowPass::OnRender(RenderCommandList& command_list)
{
    if (!m_settings.Get<bool>("use_shadow")) {
        return;
    }

    command_list.SetViewport(0, 0, m_settings.Get<float>("s_size"), m_settings.Get<float>("s_size"));

    command_list.UseProgram(m_program);
    command_list.Attach(m_program.vs.cbv.VSParams, m_program.vs.cbuffer.VSParams);
    command_list.Attach(m_program.ps.sampler.g_sampler, m_sampler);

    glm::vec4 color = { 0.0f, 0.0f, 0.0f, 1.0f };
    RenderPassBeginDesc render_pass_desc = {};
    render_pass_desc.depth_stencil.texture = output.srv;
    render_pass_desc.depth_stencil.clear_depth = 1.0f;

    command_list.BeginRenderPass(render_pass_desc);
    for (auto& model : m_input.scene_list) {
        m_program.vs.cbuffer.VSParams.World = glm::transpose(model.matrix);

        command_list.SetRasterizeState({ FillMode::kSolid, CullMode::kBack, 4096 });

        model.ia.indices.Bind(command_list);
        model.ia.positions.BindToSlot(command_list, m_program.vs.ia.SV_POSITION);
        model.ia.texcoords.BindToSlot(command_list, m_program.vs.ia.TEXCOORD);

        for (auto& range : model.ia.ranges) {
            auto& material = model.GetMaterial(range.id);

            if (m_settings.Get<bool>("shadow_discard")) {
                command_list.Attach(m_program.ps.srv.alphaMap, material.texture.opacity);
            } else {
                command_list.Attach(m_program.ps.srv.alphaMap);
            }

            command_list.DrawIndexed(range.index_count, 6, range.start_index_location, range.base_vertex_location, 0);
        }
    }
    command_list.EndRenderPass();
}

void ShadowPass::CreateSizeDependentResources()
{
    output.srv = m_device.CreateTexture(BindFlag::kDepthStencil | BindFlag::kShaderResource,
                                        gli::format::FORMAT_D32_SFLOAT_PACK32, 1, m_settings.Get<float>("s_size"),
                                        m_settings.Get<float>("s_size"), 6);
}

void ShadowPass::OnModifySponzaSettings(const SponzaSettings& settings)
{
    SponzaSettings prev = m_settings;
    m_settings = settings;
    if (prev.Get<float>("s_size") != m_settings.Get<float>("s_size")) {
        CreateSizeDependentResources();
    }
}
