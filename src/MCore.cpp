#include <assert.h>

#include "Server.hpp"
#include "util/Logs.hpp"

#include "backend/wayland/BackendWayland.hpp"
#include "vulkan/PhysDevSel.hpp"
#include "vulkan/VlkInstance.hpp"

int main()
{
    SetupLogging();

    {
        VlkInstance vk( VlkInstanceType::Wayland );
        BackendWayland backend( vk );
        auto dev = PhysDevSel::PickBest( vk.QueryPhysicalDevices(), backend, PhysDevSel::RequireGraphic );
        assert( dev );
        backend.Run();
    }

    Server server;

    return 0;
}
