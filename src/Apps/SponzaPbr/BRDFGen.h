#pragma once

#include "GeometryPass.h"
#include "SponzaSettings.h"
#include <Device/Device.h>
#include <Geometry/Geometry.h>
#include <ProgramRef/BRDF_PS.h>
#include <ProgramRef/BRDF_VS.h>

class BRDFGen : public IPass
{
public:
    struct Input
    {
        Model& square_model;
    };

    struct Output
    {
        std::shared_ptr<Resource> brdf;
    } output;

    BRDFGen(RenderDevice& device, const Input& input);

    virtual void OnUpdate() override;
    virtual void OnRender(RenderCommandList& command_list)override;
    virtual void OnResize(int width, int height) override;
    virtual void OnModifySponzaSettings(const SponzaSettings& settings) override;

private:
    void DrawBRDF(RenderCommandList& command_list);

    SponzaSettings m_settings;
    RenderDevice& m_device;
    Input m_input;
    std::shared_ptr<Resource> m_dsv;
    ProgramHolder<BRDF_VS, BRDF_PS> m_program;
    size_t m_size = 512;
    bool is = false;
};
