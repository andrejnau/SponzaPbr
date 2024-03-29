#pragma once

#include "Camera/Camera.h"
#include "Device/Device.h"
#include "Geometry/Geometry.h"
#include "ProgramRef/ShadowPass_PS.h"
#include "ProgramRef/ShadowPass_VS.h"
#include "RenderPass.h"
#include "SponzaSettings.h"

class ShadowPass : public IPass {
public:
    struct Input {
        SceneModels& scene_list;
        const Camera& camera;
        glm::vec3& light_pos;
    };

    struct Output {
        std::shared_ptr<Resource> srv;
    } output;

    ShadowPass(RenderDevice& device, const Input& input);

    virtual void OnUpdate() override;
    virtual void OnRender(RenderCommandList& command_list) override;
    virtual void OnModifySponzaSettings(const SponzaSettings& settings) override;

private:
    void CreateSizeDependentResources();

    SponzaSettings m_settings;
    RenderDevice& m_device;
    Input m_input;
    ProgramHolder<ShadowPass_VS, ShadowPass_PS> m_program;
    std::shared_ptr<Resource> m_sampler;
};
