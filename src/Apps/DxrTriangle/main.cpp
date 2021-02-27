#include <AppBox/AppBox.h>
#include <AppBox/ArgsParser.h>
#include <RenderDevice/RenderDevice.h>
#include <ProgramRef/RayTracing.h>
#include <glm/gtx/transform.hpp>
#include <stdexcept>

int main(int argc, char *argv[])
{
    Settings settings = ParseArgs(argc, argv);
    AppBox app("DxrTriangle", settings);
    AppRect rect = app.GetAppRect();

    std::shared_ptr<RenderDevice> device = CreateRenderDevice(settings, app.GetWindow());
    if (!device->IsDxrSupported())
        throw std::runtime_error("Ray Tracing is not supported");
    app.SetGpuName(device->GetGpuName());

    std::shared_ptr<RenderCommandList> upload_command_list = device->CreateRenderCommandList();

    std::vector<glm::vec3> positions_data = {
        glm::vec3(-0.5, -0.5, 0.0),
        glm::vec3(0.0, 0.5, 0.0),
        glm::vec3(0.5, -0.5, 0.0)
    };
    std::shared_ptr<Resource> positions = device->CreateBuffer(BindFlag::kVertexBuffer | BindFlag::kCopyDest, sizeof(glm::vec3) * positions_data.size());
    upload_command_list->UpdateSubresource(positions, 0, positions_data.data(), 0, 0);
    RaytracingGeometryDesc raytracing_geometry_desc = { { positions, gli::format::FORMAT_RGB32_SFLOAT_PACK32, 3 }, {}, RaytracingGeometryFlags::kOpaque };
    std::shared_ptr<Resource> bottom = device->CreateBottomLevelAS({ raytracing_geometry_desc }, BuildAccelerationStructureFlags::kAllowCompaction);
    upload_command_list->BuildBottomLevelAS({}, bottom, { raytracing_geometry_desc }, BuildAccelerationStructureFlags::kAllowCompaction);
    upload_command_list->CopyAccelerationStructure(bottom, bottom, CopyAccelerationStructureMode::kCompact);
    std::vector<std::pair<std::shared_ptr<Resource>, glm::mat4>> geometry = {
        { bottom, glm::mat4(1.0) },
    };
    std::shared_ptr<Resource> top = device->CreateTopLevelAS(geometry.size(), BuildAccelerationStructureFlags::kNone);
    upload_command_list->BuildTopLevelAS({}, top, geometry);
    std::shared_ptr<Resource> uav = device->CreateTexture(BindFlag::kUnorderedAccess | BindFlag::kShaderResource | BindFlag::kCopySource,
                                                          device->GetFormat(), 1, rect.width, rect.height);
    upload_command_list->Close();
    device->ExecuteCommandLists({ upload_command_list });

    ProgramHolder<RayTracing> program(*device);
    std::vector<std::shared_ptr<RenderCommandList>> command_lists;
    for (uint32_t i = 0; i < settings.frame_count; ++i)
    {
        decltype(auto) command_list = device->CreateRenderCommandList();
        command_list->UseProgram(program);
        command_list->Attach(program.lib.srv.geometry, top);
        command_list->Attach(program.lib.uav.result, uav);
        command_list->DispatchRays(rect.width, rect.height, 1);
        command_list->CopyTexture(uav, device->GetBackBuffer(i), { { rect.width, rect.height, 1 } });
        command_list->Close();
        command_lists.emplace_back(command_list);
    }

    while (!app.PollEvents())
    {
        device->ExecuteCommandLists({ command_lists[device->GetFrameIndex()] });
        device->Present();
    }
    return 0;
}
