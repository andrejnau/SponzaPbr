#pragma once

#include "SponzaSettings.h"
#include "RenderPass.h"
#include <Device/Device.h>
#include <Geometry/Geometry.h>
#include <ProgramRef/HDRLum1DPass_CS.h>
#include <ProgramRef/HDRLum2DPass_CS.h>
#include <ProgramRef/HDRApply_PS.h>
#include <ProgramRef/HDRApply_VS.h>

class ComputeLuminance : public IPass
{
public:
    struct Input
    {
        std::shared_ptr<Resource>& hdr_res;
        Model& model;
        std::shared_ptr<Resource>& rtv;
        std::shared_ptr<Resource>& dsv;
    };

    struct Output
    {
    } output;

    ComputeLuminance(RenderDevice& device, const Input& input, int width, int height);

    virtual void OnUpdate() override;
    virtual void OnRender(RenderCommandList& command_list)override;
    virtual void OnResize(int width, int height) override;
    virtual void OnModifySponzaSettings(const SponzaSettings& settings) override;

private:
    void GetLum2DPass_CS(RenderCommandList& command_list, size_t buf_id, uint32_t thread_group_x, uint32_t thread_group_y);
    void GetLum1DPass_CS(RenderCommandList& command_list, size_t buf_id, uint32_t input_buffer_size, uint32_t thread_group_x);
    void CreateBuffers();
    uint32_t m_thread_group_x;
    uint32_t m_thread_group_y;

    void Draw(RenderCommandList& command_list, size_t buf_id);

    SponzaSettings m_settings;
    RenderDevice& m_device;
    Input m_input;
    int m_width;
    int m_height;
    ProgramHolder<HDRLum1DPass_CS> m_HDRLum1DPass_CS;
    ProgramHolder<HDRLum2DPass_CS> m_HDRLum2DPass_CS;
    ProgramHolder<HDRApply_PS, HDRApply_VS> m_HDRApply;
    std::vector<std::shared_ptr<Resource>> m_use_res;
};
