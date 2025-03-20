#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#include <wayland-client.h>

#include "util/NoCopy.hpp"
#include "util/RobinHood.hpp"
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
enum class WaylandCursor;
class WaylandDisplay;
struct WaylandScroll;

class WaylandWindow : public GarbageChute
{
    friend class WaylandSeat;

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
        void (*OnClose)( void* ptr );
        bool (*OnRender)( void* ptr );
        void (*OnScale)( void* ptr, uint32_t width, uint32_t height, uint32_t scale );  // Logical pixels
        void (*OnResize)( void* ptr, uint32_t width, uint32_t height );                 // Logical pixels
        void (*OnFormatChange)( void* ptr, VkFormat format );
        void (*OnClipboard)( void* ptr, const unordered_flat_set<std::string>& mimeTypes );
        void (*OnDrag)( void* ptr, const unordered_flat_set<std::string>& mimeTypes );
        void (*OnDrop)( void* ptr, int fd, const char* mime );
        void (*OnKey)( void* ptr, const char* key, int mods );
        void (*OnMouseEnter)( void* ptr, float x, float y );
        void (*OnMouseLeave)( void* ptr );
        void (*OnMouseMove)( void* ptr, float x, float y );
        void (*OnMouseButton)( void* ptr, uint32_t button, bool pressed );
        void (*OnScroll)( void* ptr, const WaylandScroll& scroll );
    };

    WaylandWindow( WaylandDisplay& display, VlkInstance& vkInstance );
    ~WaylandWindow();

    NoCopy( WaylandWindow );

    void SetAppId( const char* appId );
    void SetTitle( const char* title );
    void SetIcon( const SvgImage& icon );
    void Resize( uint32_t width, uint32_t height, bool reposition = false );             // Window size in real pixels
    void ResizeNoScale( uint32_t width, uint32_t height, bool reposition = false );      // Window size in logical pixels (1.0 scale)
    void LockSize();
    void Fullscreen( bool enable );
    void Commit();
    void Close();
    void Activate( const char* token );
    void EnableHdr( bool enable );

    void Update();
    VlkCommandBuffer& BeginFrame();
    void EndFrame();

    VkImage GetImage();
    VkImageView GetImageView();
    VkFormat GetFormat();

    void SetListener( const Listener* listener, void* listenerPtr );
    void SetDevice( std::shared_ptr<VlkDevice> device );
    void SetCursor( WaylandCursor cursor );

    void InvokeRender();
    void ResumeIfIdle();

    [[nodiscard]] const VkExtent2D& GetSize() const { return m_swapchain->GetExtent(); }    // Swapchain extent, i.e. render area in real pixels
    [[nodiscard]] const VkExtent2D& GetSizeNoScale() const { return m_extent; }             // Logical window size, i.e. pixels at 1.0 DPI scaling
    [[nodiscard]] const char* GetTitle() const { return m_title.c_str(); }
    [[nodiscard]] uint32_t GetScale() const { return m_scale; }
    [[nodiscard]] VkExtent2D GetBounds() const { return VkExtent2D { m_bounds.width * m_scale / 120, m_bounds.height * m_scale / 120 }; }
    [[nodiscard]] const VkExtent2D& GetBoundsNoScale() const { return m_bounds; }
    [[nodiscard]] bool HdrCapable() const { return m_hdrCapable; }
    [[nodiscard]] bool IsMaximized() const { return m_maximized; }
    [[nodiscard]] bool IsFullscreen() const { return m_fullscreen; }

    [[nodiscard]] wl_surface* Surface() { return m_surface; }
    [[nodiscard]] xdg_toplevel* XdgToplevel() { return m_xdgToplevel; }
    [[nodiscard]] VkSurfaceKHR VkSurface() { return *m_vkSurface; }
    [[nodiscard]] VlkDevice& Device() { return *m_vkDevice; }

    [[nodiscard]] int GetClipboard( const char* mime );
    void AcceptDndMime( const char* mime );
    void FinishDnd( int fd );

    void Recycle( std::shared_ptr<VlkBase>&& garbage ) override;
    void Recycle( std::vector<std::shared_ptr<VlkBase>>&& garbage ) override;

    void lock() { m_stateLock.lock(); }
    void unlock() { m_stateLock.unlock(); }

private:
    void Destroy();

    void InvokeClipboard( const unordered_flat_set<std::string>& mimeTypes );
    void InvokeDrag( const unordered_flat_set<std::string>& mimeTypes );
    void InvokeDrop( int fd, const char* mime );
    void InvokeKey( const char* key, int mods );
    void InvokeMouseEnter( float x, float y );
    void InvokeMouseLeave();
    void InvokeMouseMove( float x, float y );
    void InvokeMouseButton( uint32_t button, bool pressed );
    void InvokeScroll( const WaylandScroll& scroll );

    void CreateSwapchain( const VkExtent2D& extent );
    void CleanupSwapchain( bool withSurface = false );

    void XdgSurfaceConfigure( struct xdg_surface *xdg_surface, uint32_t serial );

    void XdgToplevelConfigure( struct xdg_toplevel* toplevel, int32_t width, int32_t height, struct wl_array* states );
    void XdgToplevelClose( struct xdg_toplevel* toplevel );
    void XdgToplevelConfigureBounds( struct xdg_toplevel* toplevel, int32_t width, int32_t height );

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
    std::atomic<std::shared_ptr<VlkFence>> m_currentRenderFence;

    std::mutex m_stateLock;

    bool m_hdrCapable;

    uint32_t m_scale = 120;
    uint32_t m_prevScale = 0;

    bool m_hdr = false;
    bool m_prevHdr = false;

    VkExtent2D m_extent;
    VkExtent2D m_staged;
    VkExtent2D m_bounds;
    bool m_maximized = false;
    bool m_fullscreen = false;

    std::mutex m_idleLock;
    bool m_idle = false;

    std::mutex m_cursorLock;
    WaylandCursor m_cursor;

    std::string m_title;
};
