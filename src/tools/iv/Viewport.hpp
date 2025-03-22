#pragma once

#include <memory>
#include <mutex>
#include <stdint.h>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#include "ImageProvider.hpp"
#include "util/RobinHood.hpp"
#include "util/Vector2.hpp"

class Background;
class BusyIndicator;
class DataBuffer;
class ImageView;
class TaskDispatch;
class VlkDevice;
class VlkInstance;
class WaylandDisplay;
struct WaylandScroll;
class WaylandWindow;

class Viewport
{
public:
    Viewport( WaylandDisplay& display, VlkInstance& vkInstance, int gpu );
    ~Viewport();

    void LoadImage( const char* path );
    void LoadImage( std::unique_ptr<DataBuffer>&& data );
    void LoadImage( int fd, int flags = 0 );

private:
    void SetBusy( int64_t job );
    void Update( float delta );

    void WantRender();

    void Close();
    bool Render();
    void Scale( uint32_t width, uint32_t height, uint32_t scale );
    void Resize( uint32_t width, uint32_t height );
    void FormatChange( VkFormat format );
    void Clipboard( const unordered_flat_set<std::string>& mimeTypes );
    void Drag( const unordered_flat_set<std::string>& mimeTypes );
    void Drop( int fd, const char* mime );
    void KeyEvent( uint32_t key, int mods, bool pressed );
    void MouseEnter( float x, float y );
    void MouseLeave();
    void MouseMove( float x, float y );
    void MouseButton( uint32_t button, bool pressed );
    void Scroll( const WaylandScroll& scroll );

    void ImageHandler( int64_t id, ImageProvider::Result result, int flags, const ImageProvider::ReturnData& data );
    void ViewScaleChanged( float scale );

    void PasteClipboard();
    [[nodiscard]] std::vector<std::string> ProcessUriList( std::string uriList );
    [[nodiscard]] std::string FindValidFile( const std::vector<std::string>& uriList );

    WaylandDisplay& m_display;
    VlkInstance& m_vkInstance;

    std::unique_ptr<TaskDispatch> m_td;

    std::shared_ptr<WaylandWindow> m_window;
    std::shared_ptr<VlkDevice> m_device;

    std::shared_ptr<Background> m_background;
    std::shared_ptr<BusyIndicator> m_busyIndicator;

    std::shared_ptr<ImageProvider> m_provider;
    std::shared_ptr<ImageView> m_view;

    uint64_t m_lastTime = 0;
    bool m_render = true;

    unordered_flat_set<std::string> m_clipboardOffer;

    std::mutex m_lock;
    bool m_isBusy = false;
    int m_currentJob = -1;

    Vector2<float> m_mousePos;
    bool m_mouseFocus = false;
    bool m_dragActive = false;

    bool m_updateTitle = false;
    std::string m_origin;
    std::string m_loadOrigin;
    float m_viewScale;
};
