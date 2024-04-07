#include <getopt.h>
#include <memory>
#include <thread>

#include "image/ImageLoader.hpp"
#include "render/Texture.hpp"
#include "util/ArgParser.hpp"
#include "util/Bitmap.hpp"
#include "util/Callstack.hpp"
#include "util/Logs.hpp"
#include "util/Panic.hpp"
#include "util/TaskDispatch.hpp"
#include "vulkan/VlkCommandBuffer.hpp"
#include "vulkan/VlkDevice.hpp"
#include "vulkan/VlkInstance.hpp"
#include "vulkan/ext/PhysDevSel.hpp"
#include "wayland/WaylandDisplay.hpp"
#include "wayland/WaylandWindow.hpp"


std::unique_ptr<TaskDispatch> g_dispatch;
std::unique_ptr<VlkInstance> g_vkInstance;
std::unique_ptr<WaylandDisplay> g_waylandDisplay;
std::unique_ptr<WaylandWindow> g_waylandWindow;
std::shared_ptr<VlkDevice> g_vkDevice;
std::unique_ptr<Bitmap> g_bitmap;
std::unique_ptr<Texture> g_texture;


void Render( void*, WaylandWindow* window )
{
    auto& cmdbuf = window->BeginFrame();

    VkRenderingAttachmentInfo attachmentInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = window->GetImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {{{ 0.25f, 0.25f, 0.25f, 1.0f }}}
    };

    VkRenderingInfo renderingInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = { { 0, 0 }, window->GetExtent() },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachmentInfo
    };

    vkCmdBeginRendering( cmdbuf, &renderingInfo );
    VkImageBlit region = {
        .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .srcOffsets = { { 0, 0, 0 }, { int32_t( g_bitmap->Width() ), int32_t( g_bitmap->Height() ), 1 } },
        .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .dstOffsets = { { 0, 0, 0 }, { int32_t( window->GetExtent().width ), int32_t( window->GetExtent().height ), 1 } }
    };
    vkCmdBlitImage( cmdbuf, *g_texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, g_waylandWindow->GetImage(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, &region, VK_FILTER_NEAREST );
    vkCmdEndRendering( cmdbuf );

    window->EndFrame();
}

int main( int argc, char** argv )
{
#ifdef NDEBUG
    SetLogLevel( LogLevel::Error );
#endif

#ifdef DEBUG
    bool enableValidation = true;
#else
    bool enableValidation = false;
#endif

    struct option longOptions[] = {
        { "debug", no_argument, nullptr, 'd' },
        { "external", no_argument, nullptr, 'e' },
        { "validation", required_argument, nullptr, 'V' },
    };

    int opt;
    while( ( opt = getopt_long( argc, argv, "deV:", longOptions, nullptr ) ) != -1 )
    {
        switch (opt)
        {
        case 'd':
            SetLogLevel( LogLevel::Callstack );
            break;
        case 'e':
            ShowExternalCallstacks( true );
            break;
        case 'V':
            enableValidation = ParseBoolean( optarg );
            break;
        default:
            break;
        }
    }
    if (optind == argc)
    {
        mclog( LogLevel::Error, "Image file name must be provided" );
        return 1;
    }

    const char* imageFile = argv[optind];

    auto vulkanThread = std::thread( [enableValidation] {
        g_vkInstance = std::make_unique<VlkInstance>( VlkInstanceType::Wayland, enableValidation );
    } );
    auto dispatchThread = std::thread( [] {
        const auto cpus = std::thread::hardware_concurrency();
        g_dispatch = std::make_unique<TaskDispatch>( cpus == 0 ? 0 : cpus - 1, "Worker" );
    } );
    auto imageThread = std::thread( [imageFile] {
        g_bitmap.reset( LoadImage( imageFile ) );
        CheckPanic( g_bitmap, "Failed to load image" );
    } );

    g_waylandDisplay = std::make_unique<WaylandDisplay>();
    g_waylandDisplay->Connect();

    dispatchThread.join();
    vulkanThread.join();

    static constexpr WaylandWindow::Listener listener = {
        .OnClose = [] (void*, WaylandWindow*) { g_waylandDisplay->Stop(); },
        .OnRender = Render,
        .OnResize = [] (void*, WaylandWindow*, uint32_t width, uint32_t height) { g_waylandWindow->Resize( width, height ); }
    };

    // Sync is being performed in InitPhysicalDevices
    g_dispatch->Queue( [] {
        g_waylandWindow = std::make_unique<WaylandWindow>( *g_waylandDisplay, *g_vkInstance );
        g_waylandWindow->SetListener( &listener, nullptr );
        g_waylandWindow->SetTitle( "AFIV" );
        g_waylandWindow->SetAppId( "afiv" );
    } );

    g_vkInstance->InitPhysicalDevices( *g_dispatch );
    const auto& devices = g_vkInstance->QueryPhysicalDevices();
    CheckPanic( !devices.empty(), "No physical devices found" );
    mclog( LogLevel::Info, "Found %d physical devices", devices.size() );

    auto best = PhysDevSel::PickBest( g_vkInstance->QueryPhysicalDevices(), g_waylandWindow->VkSurface(), PhysDevSel::RequireGraphic );
    CheckPanic( best, "Failed to find suitable physical device" );

    g_vkDevice = std::make_shared<VlkDevice>( *g_vkInstance, best, VlkDevice::RequireGraphic | VlkDevice::RequirePresent, g_waylandWindow->VkSurface() );
    imageThread.join();
    g_waylandWindow->SetDevice( g_vkDevice, VkExtent2D( g_bitmap->Width(), g_bitmap->Height() ) );

    g_texture = std::make_unique<Texture>( *g_vkDevice, *g_bitmap, VK_FORMAT_R8G8B8A8_SRGB );

    g_waylandWindow->Commit( true );
    g_waylandDisplay->Run();

    return 0;
}
