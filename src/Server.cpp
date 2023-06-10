#include <stdlib.h>

#include "Server.hpp"
#include "backend/wayland/BackendWayland.hpp"
#include "plumbing/Display.hpp"
#include "util/Logs.hpp"
#include "util/Panic.hpp"
#include "vulkan/PhysDevSel.hpp"
#include "vulkan/VlkDevice.hpp"
#include "vulkan/VlkInfo.hpp"
#include "vulkan/VlkInstance.hpp"
#include "vulkan/VlkSwapchain.hpp"

Server::Server()
{
    const auto waylandDpy = getenv( "WAYLAND_DISPLAY" );
    if( waylandDpy )
    {
        mclog( LogLevel::Info, "Running on Wayland display: %s", waylandDpy );

        m_vkInstance = std::make_unique<VlkInstance>( VlkInstanceType::Wayland );
        m_backend = std::make_unique<BackendWayland>( *m_vkInstance );
    }
    else
    {
        mclog( LogLevel::Fatal, "Direct rendering is not implemented yet" );
        abort();
    }

    auto physDev = PhysDevSel::PickBest( m_vkInstance->QueryPhysicalDevices(), *m_backend, PhysDevSel::RequireGraphic );
    CheckPanic( physDev, "No suitable GPU found" );
#ifdef DEBUG
    PrintVulkanPhysicalDevice( physDev );
#endif

    m_vkDevice = std::make_unique<VlkDevice>( *m_vkInstance, physDev, VlkDevice::RequireGraphic | VlkDevice::RequirePresent, *m_backend );
#ifdef DEBUG
    PrintVulkanQueues( *m_vkDevice );
#endif

    VlkSwapchain swapchain( physDev, *m_backend );

    m_dpy = std::make_unique<Display>();
    setenv( "WAYLAND_DISPLAY", m_dpy->Socket(), 1 );
}

void Server::Run()
{
    m_backend->Run();
}

Server::~Server()
{
}
