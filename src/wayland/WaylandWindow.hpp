#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>
#include <wayland-client.h>

#include "util/NoCopy.hpp"
#include "vulkan/VlkSurface.hpp"
#include "vulkan/VlkSwapchain.hpp"
#include "vulkan/ext/GarbageChute.hpp"

#include "wayland-xdg-decoration-client-protocol.h"
#include "wayland-xdg-shell-client-protocol.h"
#include "wayland-fractional-scale-client-protocol.h"
#include "wayland-viewporter-client-protocol.h"

class SvgImage;
class VlkCommandBuffer;
class VlkDevice;
class VlkFence;
class VlkInstance;
class VlkSemaphore;
class WaylandDisplay;

class WaylandWindow : public GarbageChute
{
    struct FrameData
    {
        std::shared_ptr<VlkCommandBuffer> commandBuffer;
        std::shared_ptr<VlkSemaphore> imageAvailable;
        std::shared_ptr<VlkSemaphore> renderFinished;
        std::shared_ptr<VlkFence> renderFence;
        std::shared_ptr<VlkFence> presentFence;
    };

public:
    struct Listener
    {
        void (*OnClose)( void* ptr, WaylandWindow* window );
        bool (*OnRender)( void* ptr, WaylandWindow* window );
        void (*OnScale)( void* ptr, WaylandWindow* window, uint32_t scale );
        void (*OnResize)( void* ptr, WaylandWindow* window, uint32_t width, uint32_t height );
    };

    WaylandWindow( WaylandDisplay& display, VlkInstance& vkInstance );
    ~WaylandWindow();

    NoCopy( WaylandWindow );

    void SetAppId( const char* appId );
    void SetTitle( const char* title );
    void SetIcon( const SvgImage& icon );
    void Resize( uint32_t width, uint32_t height );
    void LockSize();
    void Commit();
    void Close();

    VlkCommandBuffer& BeginFrame();
    void EndFrame();

    VkImage GetImage();
    VkImageView GetImageView();
    VkFormat GetFormat();

    void SetListener( const Listener* listener, void* listenerPtr );
    void SetDevice( std::shared_ptr<VlkDevice> device, const VkExtent2D& extent );

    void InvokeRender();

    [[nodiscard]] const VkExtent2D& GetExtent() const { return m_swapchain->GetExtent(); }
    [[nodiscard]] const char* GetTitle() const { return m_title.c_str(); }

    [[nodiscard]] wl_surface* Surface() { return m_surface; }
    [[nodiscard]] xdg_toplevel* XdgToplevel() { return m_xdgToplevel; }
    [[nodiscard]] VkSurfaceKHR VkSurface() { return *m_vkSurface; }
    [[nodiscard]] VlkDevice& Device() { return *m_vkDevice; }

    void Recycle( std::shared_ptr<VlkBase>&& garbage ) override;
    void Recycle( std::vector<std::shared_ptr<VlkBase>>&& garbage ) override;

private:
    void CreateSwapchain( const VkExtent2D& extent );
    void CleanupSwapchain( bool withSurface = false );

    void XdgSurfaceConfigure( struct xdg_surface *xdg_surface, uint32_t serial );

    void XdgToplevelConfigure( struct xdg_toplevel* toplevel, int32_t width, int32_t height, struct wl_array* states );
    void XdgToplevelClose( struct xdg_toplevel* toplevel );

    void DecorationConfigure( zxdg_toplevel_decoration_v1* tldec, uint32_t mode );

    void FractionalScalePreferredScale( wp_fractional_scale_v1* scale, uint32_t scaleValue );

    void FrameDone( struct wl_callback* cb, uint32_t time );

    WaylandDisplay& m_display;
    wl_surface* m_surface;
    xdg_surface* m_xdgSurface;
    xdg_toplevel* m_xdgToplevel;
    zxdg_toplevel_decoration_v1* m_xdgToplevelDecoration = nullptr;
    wp_fractional_scale_v1* m_fractionalScale = nullptr;
    wp_viewport* m_viewport = nullptr;

    VlkInstance& m_vkInstance;
    std::shared_ptr<VlkDevice> m_vkDevice;
    std::shared_ptr<VlkSwapchain> m_swapchain;
    std::shared_ptr<VlkSurface> m_vkSurface;

    const Listener* m_listener = nullptr;
    void* m_listenerPtr;

    std::vector<FrameData> m_frameData;
    uint32_t m_frameIdx = 0;
    uint32_t m_imageIdx;

    uint32_t m_scale = 120;
    uint32_t m_prevScale = 0;

    VkExtent2D m_extent;
    VkExtent2D m_staged;

    std::string m_title;
};
