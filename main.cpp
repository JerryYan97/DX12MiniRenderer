#include "MiniRendererApp.h"

int main()
{
    DX12MiniRenderer renderer;
    renderer.Init();
    renderer.Run();
    renderer.Finalize();

    return 0;
}