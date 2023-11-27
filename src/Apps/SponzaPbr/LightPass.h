#pragma once

#include "Device/Device.h"
#include "Geometry/Geometry.h"
#include "GeometryPass.h"
#include "IrradianceConversion.h"
#include "ProgramRef/LightPass_PS.h"
#include "ProgramRef/LightPass_VS.h"
#include "RenderPass.h"
#include "SSAOPass.h"
#include "ShadowPass.h"
#include "SponzaSettings.h"

class LightPass : public IPass {
public:
    struct Input {
        GeometryPass::Output& geometry_pass;
        ShadowPass::Output& shadow_pass;
        SSAOPass::Output& ssao_pass;
        std::shared_ptr<Resource>*& ray_tracing_ao;
        Model& model;
        const Camera& camera;
        glm::vec3& light_pos;
        std::shared_ptr<Resource>& irradince;
        std::shared_ptr<Resource>& prefilter;
        std::shared_ptr<Resource>& brdf;
    };

    struct Output {
        std::shared_ptr<Resource> rtv;
    } output;

    LightPass(RenderDevice& device, const Input& input, int width, int height);

    virtual void OnUpdate() override;
    virtual void OnRender(RenderCommandList& command_list) override;
    virtual void OnResize(int width, int height) override;
    virtual void OnModifySponzaSettings(const SponzaSettings& settings) override;

private:
    void CreateSizeDependentResources();
    void SetDefines(ProgramHolder<LightPass_PS, LightPass_VS>& program);

    SponzaSettings m_settings;
    RenderDevice& m_device;
    Input m_input;
    int m_width;
    int m_height;
    ProgramHolder<LightPass_PS, LightPass_VS> m_program;
    std::shared_ptr<Resource> m_depth_stencil_view;
    std::shared_ptr<Resource> m_sampler;
    std::shared_ptr<Resource> m_sampler_brdf;
    std::shared_ptr<Resource> m_compare_sampler;
};
