#pragma once

#include <memory>
#include <mutex>
#include <stdint.h>
#include <string>

#include "util/RobinHood.hpp"

class Background;
class Bitmap;
class BusyIndicator;
class DataBuffer;
class ImageProvider;
class ImageView;
class VlkDevice;
class VlkInstance;
class WaylandDisplay;
class WaylandWindow;

class Viewport
{
public:
    Viewport( WaylandDisplay& display, VlkInstance& vkInstance, int gpu );
    ~Viewport();

    void LoadImage( const char* path );
    void LoadImage( std::unique_ptr<DataBuffer>&& data );

private:
    void Close();
    bool Render();
    void Scale( uint32_t scale );
    void Resize( uint32_t width, uint32_t height );
    void Clipboard( const unordered_flat_set<std::string>& mimeTypes );

    void ImageHandler( int64_t id, int result, std::shared_ptr<Bitmap> bitmap );

    WaylandDisplay& m_display;
    VlkInstance& m_vkInstance;

    std::shared_ptr<WaylandWindow> m_window;
    std::shared_ptr<VlkDevice> m_device;

    std::shared_ptr<Background> m_background;
    std::shared_ptr<BusyIndicator> m_busyIndicator;

    std::shared_ptr<ImageProvider> m_provider;
    std::shared_ptr<ImageView> m_view;

    uint64_t m_lastTime = 0;

    unordered_flat_set<std::string> m_clipboardOffer;

    std::mutex m_lock;
    bool m_isBusy = false;
};
