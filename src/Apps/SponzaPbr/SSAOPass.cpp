#include "SSAOPass.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <chrono>
#include <random>
#include <Utilities/FormatHelper.h>

inline float lerp(float a, float b, float f)
{
    return a + f * (b - a);
}

SSAOPass::SSAOPass(RenderDevice& device, RenderCommandList& command_list, const Input& input, int width, int height)
    : m_device(device)
    , m_input(input)
    , m_width(width)
    , m_height(height)
    , m_program(device, std::bind(&SSAOPass::SetDefines, this, std::placeholders::_1))
    , m_program_blur(device)
{
    m_sampler = m_device.CreateSampler({
        SamplerFilter::kAnisotropic,
        SamplerTextureAddressMode::kWrap,
        SamplerComparisonFunc::kNever });
    CreateSizeDependentResources();

    std::uniform_real_distribution<float> randomFloats(0.0, 1.0);
    std::default_random_engine generator;
    int kernel_size = m_program.ps.cbuffer.SSAOBuffer.samples.size();
    for (int i = 0; i < kernel_size; ++i)
    {
        glm::vec3 sample(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, randomFloats(generator));
        sample = glm::normalize(sample);
        sample *= randomFloats(generator);
        float scale = float(i) / float(kernel_size);

        // Scale samples s.t. they're more aligned to center of kernel
        scale = lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;
        m_program.ps.cbuffer.SSAOBuffer.samples[i] = glm::vec4(sample, 1.0f);
    }

    std::vector<glm::vec4> ssaoNoise;
    for (uint32_t i = 0; i < 16; ++i)
    {
        glm::vec4 noise(randomFloats(generator) * 2.0f - 1.0f, randomFloats(generator) * 2.0f - 1.0f, 0.0f, 0.0f);
        ssaoNoise.push_back(noise);
    }
    m_noise_texture = device.CreateTexture(BindFlag::kShaderResource | BindFlag::kCopyDest, gli::format::FORMAT_RGBA32_SFLOAT_PACK32, 1, 4, 4);
    size_t num_bytes = 0;
    size_t row_bytes = 0;
    GetFormatInfo(4, 4, gli::format::FORMAT_RGBA32_SFLOAT_PACK32, num_bytes, row_bytes);
    command_list.UpdateSubresource(m_noise_texture, 0, ssaoNoise.data(), row_bytes, num_bytes);
}

void SSAOPass::OnUpdate()
{
    m_program.ps.cbuffer.SSAOBuffer.ao_radius = m_settings.Get<float>("ao_radius");
    m_program.ps.cbuffer.SSAOBuffer.width = m_width;
    m_program.ps.cbuffer.SSAOBuffer.height = m_height;

    glm::mat4 projection, view, model;
    m_input.camera.GetMatrix(projection, view, model);
    m_program.ps.cbuffer.SSAOBuffer.projection = glm::transpose(projection);
    m_program.ps.cbuffer.SSAOBuffer.view = glm::transpose(view);
    m_program.ps.cbuffer.SSAOBuffer.viewInverse = glm::transpose(glm::transpose(glm::inverse(m_input.camera.GetViewMatrix())));
}

void SSAOPass::OnRender(RenderCommandList& command_list)
{
    if (!m_settings.Get<bool>("use_ssao"))
        return;

    command_list.SetViewport(0, 0, m_width, m_height);

    command_list.UseProgram(m_program);
    command_list.Attach(m_program.ps.cbv.SSAOBuffer, m_program.ps.cbuffer.SSAOBuffer);

    glm::vec4 color = { 0.0f, 0.0f, 0.0f, 1.0f };
    RenderPassBeginDesc render_pass_desc = {};
    render_pass_desc.colors[m_program.ps.om.rtv0].texture = m_ao;
    render_pass_desc.colors[m_program.ps.om.rtv0].clear_color = color;
    render_pass_desc.depth_stencil.texture = m_depth_stencil_view;
    render_pass_desc.depth_stencil.clear_depth = 1.0f;

    m_input.square.ia.indices.Bind(command_list);
    m_input.square.ia.positions.BindToSlot(command_list, m_program.vs.ia.POSITION);
    m_input.square.ia.texcoords.BindToSlot(command_list, m_program.vs.ia.TEXCOORD);

    command_list.BeginRenderPass(render_pass_desc);
    for (auto& range : m_input.square.ia.ranges)
    {
        command_list.Attach(m_program.ps.srv.gPosition, m_input.geometry_pass.position);
        command_list.Attach(m_program.ps.srv.gNormal, m_input.geometry_pass.normal);
        command_list.Attach(m_program.ps.srv.noiseTexture, m_noise_texture);
        command_list.DrawIndexed(range.index_count, range.start_index_location, range.base_vertex_location);
    }
    command_list.EndRenderPass();

    if (m_settings.Get<bool>("use_ao_blur"))
    {
        command_list.UseProgram(m_program_blur);
        command_list.Attach(m_program_blur.ps.uav.out_uav, m_ao_blur);
        command_list.Attach(m_program_blur.ps.sampler.g_sampler, m_sampler);

        m_input.square.ia.indices.Bind(command_list);
        m_input.square.ia.positions.BindToSlot(command_list, m_program_blur.vs.ia.POSITION);
        m_input.square.ia.texcoords.BindToSlot(command_list, m_program_blur.vs.ia.TEXCOORD);

        command_list.BeginRenderPass({});
        for (auto& range : m_input.square.ia.ranges)
        {
            command_list.Attach(m_program_blur.ps.srv.ssaoInput, m_ao);
            command_list.DrawIndexed(range.index_count, range.start_index_location, range.base_vertex_location);
        }
        command_list.EndRenderPass();

        output.ao = m_ao_blur;
    }
    else
    {
        output.ao = m_ao;
    }
}

void SSAOPass::OnResize(int width, int height)
{
    m_width = width;
    m_height = height;
    CreateSizeDependentResources();
}

void SSAOPass::CreateSizeDependentResources()
{
    m_ao = m_device.CreateTexture(BindFlag::kRenderTarget | BindFlag::kShaderResource, gli::format::FORMAT_RGBA32_SFLOAT_PACK32, 1, m_width, m_height, 1);
    m_ao_blur = m_device.CreateTexture(BindFlag::kRenderTarget | BindFlag::kShaderResource | BindFlag::kUnorderedAccess, gli::format::FORMAT_RGBA32_SFLOAT_PACK32, 1, m_width, m_height, 1);
    m_depth_stencil_view = m_device.CreateTexture(BindFlag::kDepthStencil, gli::format::FORMAT_D32_SFLOAT_PACK32, 1, m_width, m_height, 1);
}

void SSAOPass::OnModifySponzaSettings(const SponzaSettings& settings)
{
    SponzaSettings prev = m_settings;
    m_settings = settings;
    if (prev.Get<uint32_t>("sample_count") != m_settings.Get<uint32_t>("sample_count"))
    {
        m_program.ps.desc.define["SAMPLE_COUNT"] = std::to_string(m_settings.Get<uint32_t>("sample_count"));
        m_program.UpdateProgram();
    }
}

void SSAOPass::SetDefines(ProgramHolder<SSAOPassPS, SSAOPassVS>& program)
{
    if (m_settings.Get<uint32_t>("sample_count") != 1)
        program.ps.desc.define["SAMPLE_COUNT"] = std::to_string(m_settings.Get<uint32_t>("sample_count"));
}
