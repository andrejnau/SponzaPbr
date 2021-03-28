#pragma once

#include "RenderPass.h"
#include "SponzaSettings.h"
#include <Device/Device.h>
#include <Geometry/Geometry.h>
#include <ProgramRef/Skinning_CS.h>

class SkinningPass : public IPass, public IModifySponzaSettings
{
public:
    struct Input
    {
        SceneModels& scene_list;
    };

    struct Output
    {
    } output;

    SkinningPass(RenderDevice& device, const Input& input);

    virtual void OnUpdate() override;
    virtual void OnRender(RenderCommandList& command_list)override;
    virtual void OnModifySponzaSettings(const SponzaSettings& settings) override;

private:
    SponzaSettings m_settings;
    RenderDevice& m_device;
    Input m_input;
    ProgramHolder<Skinning_CS> m_program;
};
