#include "MiniRendererApp.h"
#include "ThirdParty/arg/args.hxx"

int main(int argc, char** argv)
{
    InputInfoManager inputInfoManager;
    inputInfoManager.GatherInfo();
    std::string backendScenesSelecInfoStr = inputInfoManager.GetCurrentScenesBackendInfoStr();

    args::ArgumentParser parser("DX12 Mini-Renderer. Supports Rasterization/Raytracing Backend and various scenes.", "E.g. DX12MiniRenderer.exe --scene 1\n" + backendScenesSelecInfoStr);
    args::HelpFlag help(parser, "help", "Display this help menu", { 'h', "help" });
    args::CompletionFlag completion(parser, { "complete" });

    args::ValueFlag<int> inputSceneId(parser, "", "The render scene idx.", { 's', "scene" });

    try
    {
        parser.ParseCLI(argc, argv);
    }
    catch (const args::Completion& e)
    {
        std::cout << e.what();
        return 0;
    }
    catch (const args::Help&)
    {
        std::cout << parser;
        return 0;
    }
    catch (const args::ParseError& e)
    {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }

    std::string sceneYmlFilePath = "";
    if (inputSceneId)
    {
        int sceneId = inputSceneId.Get();
        sceneYmlFilePath = inputInfoManager.GetBackendSceneFilePath(sceneId);
    }

    if (sceneYmlFilePath == "")
    {
        std::cerr << "No Valid and Default File." << std::endl;
        return 0;
    }
    
    DX12MiniRenderer renderer;
    renderer.Init(sceneYmlFilePath);
    renderer.Run();
    renderer.Finalize();

    return 0;
}