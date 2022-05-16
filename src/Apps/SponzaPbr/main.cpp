#include "Scene.h"
#include <AppBox/AppBox.h>
#include <AppBox/ArgsParser.h>

int main(int argc, char *argv[])
{
    Settings settings = ParseArgs(argc, argv);
    AppBox app("SponzaPbr", settings);
    AppRect rect = app.GetAppRect();
    Scene scene(settings, CreateRenderDevice(settings, app.GetNativeWindow(), rect.width, rect.height), app.GetWindow(), rect.width, rect.height);
    app.SubscribeEvents(&scene, &scene);
    app.SetGpuName(scene.GetRenderDevice().GetGpuName());
    while (!app.PollEvents())
    {
        scene.RenderFrame();
    }
    return 0;
}
