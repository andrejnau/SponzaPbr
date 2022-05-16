#pragma once

#include "SponzaSettings.h"
#include "RenderPass.h"
#include <Device/Device.h>
#include <Camera/Camera.h>
#include <Geometry/Geometry.h>
#include <ProgramRef/GeometryPass_PS.h>
#include <ProgramRef/GeometryPass_VS.h>

class GeometryPass : public IPass
{
public:
    struct Input
    {
        SceneModels& scene_list;
        const Camera& camera;
    };

    struct Output
    {
        std::shared_ptr<Resource> position;
        std::shared_ptr<Resource> normal;
        std::shared_ptr<Resource> albedo;
        std::shared_ptr<Resource> material;
        std::shared_ptr<Resource> dsv;
    } output;

    GeometryPass(RenderDevice& device, const Input& input, int width, int height);

    virtual void OnUpdate() override;
    virtual void OnRender(RenderCommandList& command_list)override;
    virtual void OnResize(int width, int height) override;
    virtual void OnModifySponzaSettings(const SponzaSettings& settings) override;

private:
    RenderDevice& m_device;
    Input m_input;
    int m_width;
    int m_height;
    ProgramHolder<GeometryPass_PS, GeometryPass_VS> m_program;

    void CreateSizeDependentResources();

    std::shared_ptr<Resource> m_sampler;
    SponzaSettings m_settings;
};
