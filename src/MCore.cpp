#include "Server.hpp"
#include "util/Logs.hpp"

#include "backend/wayland/BackendWayland.hpp"
#include "vulkan/VlkInstance.hpp"

int main()
{
    SetupLogging();

    {
        VlkInstance vk;
        BackendWayland ww;
        ww.Run();
    }

    Server server;

    return 0;
}
