#pragma once

#include "Camera/Camera.h"
#include "Device/Device.h"
#include "Geometry/Geometry.h"
#include "GeometryPass.h"
#include "ProgramRef/SSAOBlurPass_PS.h"
#include "ProgramRef/SSAOPass_PS.h"
#include "ProgramRef/SSAOPass_VS.h"
#include "SponzaSettings.h"

class SSAOPass : public IPass {
public:
    struct Input {
        GeometryPass::Output& geometry_pass;
        Model& square;
        const Camera& camera;
    };

    struct Output {
        std::shared_ptr<Resource> ao;
    } output;

    SSAOPass(RenderDevice& device, RenderCommandList& command_list, const Input& input, int width, int height);

    virtual void OnUpdate() override;
    virtual void OnRender(RenderCommandList& command_list) override;
    virtual void OnResize(int width, int height) override;
    virtual void OnModifySponzaSettings(const SponzaSettings& settings) override;

private:
    void SetDefines(ProgramHolder<SSAOPass_PS, SSAOPass_VS>& program);
    void CreateSizeDependentResources();

    SponzaSettings m_settings;
    RenderDevice& m_device;
    Input m_input;
    int m_width;
    int m_height;
    std::shared_ptr<Resource> m_noise_texture;
    std::shared_ptr<Resource> m_depth_stencil_view;
    ProgramHolder<SSAOPass_PS, SSAOPass_VS> m_program;
    ProgramHolder<SSAOBlurPass_PS, SSAOPass_VS> m_program_blur;
    std::shared_ptr<Resource> m_ao;
    std::shared_ptr<Resource> m_ao_blur;
    std::shared_ptr<Resource> m_sampler;
};
