#pragma once

#include "Device/Device.h"
#include "Geometry/Geometry.h"
#include "GeometryPass.h"
#include "ProgramRef/RayTracingAO.h"
#include "ProgramRef/SSAOBlurPass_PS.h"
#include "ProgramRef/SSAOPass_VS.h"
#include "SponzaSettings.h"

class RayTracingAOPass : public IPass {
public:
    struct Input {
        GeometryPass::Output& geometry_pass;
        SceneModels& scene_list;
        Model& square;
        const Camera& camera;
    };

    struct Output {
        std::shared_ptr<Resource> ao;
    } output;

    RayTracingAOPass(RenderDevice& device, RenderCommandList& command_list, const Input& input, int width, int height);

    virtual void OnUpdate() override;
    virtual void OnRender(RenderCommandList& command_list) override;
    virtual void OnResize(int width, int height) override;
    virtual void OnModifySponzaSettings(const SponzaSettings& settings) override;

private:
    void CreateSizeDependentResources();

    SponzaSettings m_settings;
    RenderDevice& m_device;
    Input m_input;
    int m_width;
    int m_height;
    ProgramHolder<RayTracingAO> m_raytracing_program;
    ProgramHolder<SSAOBlurPass_PS, SSAOPass_VS> m_program_blur;

    std::vector<std::shared_ptr<Resource>> m_bottom;
    std::shared_ptr<Resource> m_top;
    std::shared_ptr<Resource> m_indices;
    std::shared_ptr<Resource> m_positions;
    bool m_is_initialized = false;
    std::shared_ptr<Resource> m_ao;
    std::shared_ptr<Resource> m_ao_blur;
    std::vector<std::pair<std::shared_ptr<Resource>, glm::mat4>> m_geometry;
    std::shared_ptr<Resource> m_sampler;
    std::shared_ptr<Resource> m_buffer;
    std::vector<std::shared_ptr<View>> m_views;
};
