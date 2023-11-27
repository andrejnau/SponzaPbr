#pragma once

#include "Camera/Camera.h"
#include "Device/Device.h"
#include "Geometry/Geometry.h"
#include "ProgramRef/Background_PS.h"
#include "ProgramRef/Background_VS.h"
#include "ProgramRef/DownSample_CS.h"
#include "ProgramRef/IBLComputePrePass_PS.h"
#include "ProgramRef/IBLCompute_PS.h"
#include "ProgramRef/IBLCompute_VS.h"
#include "RenderPass.h"
#include "ShadowPass.h"
#include "SponzaSettings.h"

#include <memory>
#include <vector>

class IBLCompute : public IPass {
public:
    struct Input {
        ShadowPass::Output& shadow_pass;
        SceneModels& scene_list;
        const Camera& camera;
        glm::vec3& light_pos;
        Model& model_cube;
        std::shared_ptr<Resource>& environment;
    };

    struct Output {
    } output;

    IBLCompute(RenderDevice& device, const Input& input);

    virtual void OnUpdate() override;
    virtual void OnRender(RenderCommandList& command_list) override;
    virtual void OnModifySponzaSettings(const SponzaSettings& settings) override;

private:
    void DrawPrePass(RenderCommandList& command_list, Model& ibl_model);
    void Draw(RenderCommandList& command_list, Model& ibl_model);
    void DrawBackgroud(RenderCommandList& command_list, Model& ibl_model);
    void DrawDownSample(RenderCommandList& command_list, Model& ibl_model, size_t texture_mips);
    SponzaSettings m_settings;
    RenderDevice& m_device;
    Input m_input;
    ProgramHolder<IBLCompute_VS, IBLCompute_PS> m_program;
    ProgramHolder<IBLCompute_VS, IBLComputePrePass_PS> m_program_pre_pass;
    ProgramHolder<Background_VS, Background_PS> m_program_backgroud;
    ProgramHolder<DownSample_CS> m_program_downsample;
    std::shared_ptr<Resource> m_dsv;
    std::shared_ptr<Resource> m_sampler;
    std::shared_ptr<Resource> m_compare_sampler;
    size_t m_size = 512;
    bool m_use_pre_pass = true;
};
