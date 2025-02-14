#include <getopt.h>
#include <memory>
#include <string.h>
#include <thread>

#include "image/ImageLoader.hpp"
#include "render/Texture.hpp"
#include "util/ArgParser.hpp"
#include "util/Bitmap.hpp"
#include "util/BitmapHdr.hpp"
#include "util/Callstack.hpp"
#include "util/Logs.hpp"
#include "util/Panic.hpp"
#include "util/TaskDispatch.hpp"
#include "util/Tonemapper.hpp"
#include "util/VectorImage.hpp"
#include "vulkan/VlkCommandBuffer.hpp"
#include "vulkan/VlkDevice.hpp"
#include "vulkan/VlkInstance.hpp"
#include "vulkan/ext/PhysDevSel.hpp"
#include "wayland/WaylandDisplay.hpp"
#include "wayland/WaylandWindow.hpp"


std::unique_ptr<VlkInstance> g_vkInstance;
std::unique_ptr<WaylandDisplay> g_waylandDisplay;
std::unique_ptr<WaylandWindow> g_waylandWindow;
std::shared_ptr<VlkDevice> g_vkDevice;
std::unique_ptr<Bitmap> g_bitmap;
std::unique_ptr<Texture> g_texture;
bool g_ready = false;


void Render()
{
    auto& cmdbuf = g_waylandWindow->BeginFrame( true );

    g_texture->TransitionLayout( cmdbuf, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );

    VkImageBlit region = {
        .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .srcOffsets = { { 0, 0, 0 }, { int32_t( g_bitmap->Width() ), int32_t( g_bitmap->Height() ), 1 } },
        .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .dstOffsets = { { 0, 0, 0 }, { int32_t( g_waylandWindow->GetExtent().width ), int32_t( g_waylandWindow->GetExtent().height ), 1 } }
    };
    vkCmdBlitImage( cmdbuf, *g_texture, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, g_waylandWindow->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_NEAREST );

    g_waylandWindow->EndFrame();
}

int main( int argc, char** argv )
{
#ifdef NDEBUG
    SetLogLevel( LogLevel::Error );
#endif

#ifdef NDEBUG
    bool enableValidation = false;
#else
    bool enableValidation = true;
#endif

    ToneMap::Operator tonemap = ToneMap::Operator::PbrNeutral;

    struct option longOptions[] = {
        { "debug", no_argument, nullptr, 'd' },
        { "external", no_argument, nullptr, 'e' },
        { "validation", required_argument, nullptr, 'V' },
        { "tonemap", required_argument, nullptr, 't' },
    };

    int opt;
    while( ( opt = getopt_long( argc, argv, "deV:t:", longOptions, nullptr ) ) != -1 )
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
        case 't':
            if( strcmp( optarg, "pbr" ) == 0 )
            {
                tonemap = ToneMap::Operator::PbrNeutral;
            }
            else if( strcmp( optarg, "agx" ) == 0 )
            {
                tonemap = ToneMap::Operator::AgX;
            }
            else if( strcmp( optarg, "agx-golden" ) == 0 )
            {
                tonemap = ToneMap::Operator::AgXGolden;
            }
            else if( strcmp( optarg, "agx-punchy" ) == 0 )
            {
                tonemap = ToneMap::Operator::AgXPunchy;
            }
            else
            {
                mclog( LogLevel::Error, "Unknown tone mapping operator" );
                return 1;
            }
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

    const auto workerThreads = std::max( 1u, std::thread::hardware_concurrency() - 1 );
    TaskDispatch td( workerThreads, "Worker" );

    auto vulkanThread = std::thread( [enableValidation] {
        g_vkInstance = std::make_unique<VlkInstance>( VlkInstanceType::Wayland, enableValidation );
    } );
    auto imageThread = std::thread( [imageFile, &td, tonemap] {
        auto loader = GetImageLoader( imageFile, tonemap, &td );
        if( loader )
        {
            if( loader->IsHdr() && loader->PreferHdr() )
            {
                auto hdr = loader->LoadHdr();
                g_bitmap = std::make_unique<Bitmap>( hdr->Width(), hdr->Height() );

                auto src = hdr->Data();
                auto dst = g_bitmap->Data();
                size_t sz = hdr->Width() * hdr->Height();
                while( sz )
                {
                    const auto chunk = std::min( sz, size_t( 16 * 1024 ) );
                    td.Queue( [src, dst, chunk, tonemap] {
                        ToneMap::Process( tonemap, (uint32_t*)dst, src, chunk );
                    } );
                    src += chunk * 4;
                    dst += chunk * 4;
                    sz -= chunk;
                }
                td.Sync();
            }
            else
            {
                g_bitmap = loader->Load();
            }
            if( !g_bitmap )
            {
                auto vector = LoadVectorImage( imageFile );
                if( vector )
                {
                    auto w = vector->Width();
                    auto h = vector->Height();
                    if( w < 0 || h < 0 )
                    {
                        w = 512;
                        h = 512;
                    }
                    g_bitmap = vector->Rasterize( w, h );
                }
            }
        }
        CheckPanic( g_bitmap, "Failed to load image" );
    } );

    g_waylandDisplay = std::make_unique<WaylandDisplay>();
    g_waylandDisplay->Connect();

    static constexpr WaylandWindow::Listener listener = {
        .OnClose = [] (void*, WaylandWindow*) { g_waylandDisplay->Stop(); },
        .OnRender = [] (void*, WaylandWindow*) { if( g_ready ) Render(); return true; },
        .OnResize = [] (void*, WaylandWindow*, uint32_t width, uint32_t height) { g_waylandWindow->Resize( width, height ); }
    };

    vulkanThread.join();

    g_waylandWindow = std::make_unique<WaylandWindow>( *g_waylandDisplay, *g_vkInstance );
    g_waylandWindow->SetListener( &listener, nullptr );
    g_waylandWindow->SetTitle( "IV" );
    g_waylandWindow->SetAppId( "iv" );
    g_waylandWindow->Commit();
    g_waylandDisplay->Roundtrip();

    const auto& devices = g_vkInstance->QueryPhysicalDevices();
    CheckPanic( !devices.empty(), "No physical devices found" );
    mclog( LogLevel::Info, "Found %d physical devices", devices.size() );

    auto best = PhysDevSel::PickBest( g_vkInstance->QueryPhysicalDevices(), g_waylandWindow->VkSurface(), PhysDevSel::RequireGraphic );
    CheckPanic( best, "Failed to find suitable physical device" );

    g_vkDevice = std::make_shared<VlkDevice>( *g_vkInstance, best, VlkDevice::RequireGraphic | VlkDevice::RequirePresent, g_waylandWindow->VkSurface() );
    imageThread.join();
    g_waylandWindow->SetDevice( g_vkDevice, VkExtent2D( g_bitmap->Width(), g_bitmap->Height() ) );

    g_texture = std::make_unique<Texture>( *g_vkDevice, *g_bitmap, VK_FORMAT_R8G8B8A8_SRGB );

    g_ready = true;
    g_waylandWindow->InvokeRender();
    g_waylandDisplay->Run();

    g_waylandWindow.reset();

    return 0;
}
