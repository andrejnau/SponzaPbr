#include "AppBox/AppBox.h"
#include "AppSettings/ArgsParser.h"
#include "Scene.h"

int main(int argc, char* argv[])
{
    Settings settings = ParseArgs(argc, argv);
    AppBox app("SponzaPbr", settings);
    std::shared_ptr<RenderDevice> device =
        CreateRenderDevice(settings, app.GetNativeWindow(), app.GetAppSize().width(), app.GetAppSize().height());
    Scene scene(app, settings, std::move(device));
    while (!app.PollEvents()) {
        scene.RenderFrame();
    }
    return 0;
}
