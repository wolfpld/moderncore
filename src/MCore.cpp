#include "Server.hpp"
#include "util/Logs.hpp"

#include "backend/wayland/BackendWayland.hpp"
#include "vulkan/VlkInstance.hpp"

int main()
{
    SetupLogging();

    {
        VlkInstance vk( VlkInstanceType::Wayland );
        BackendWayland backend( vk );
        backend.Run();
    }

    Server server;

    return 0;
}
