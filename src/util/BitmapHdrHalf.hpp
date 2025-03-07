#pragma once

#include <memory>
#include <stdint.h>

#include "Colorspace.hpp"
#include "NoCopy.hpp"
#include "Tonemapper.hpp"

namespace half_float { class half; }

class BitmapHdr;
class TaskDispatch;

class BitmapHdrHalf
{
public:
    explicit BitmapHdrHalf( const BitmapHdr& bmp );
    BitmapHdrHalf( uint32_t width, uint32_t height, Colorspace colorspace );
    ~BitmapHdrHalf();
    NoCopy( BitmapHdrHalf );

    void Resize( uint32_t width, uint32_t height, TaskDispatch* td = nullptr );
    [[nodiscard]] std::unique_ptr<BitmapHdrHalf> ResizeNew( uint32_t width, uint32_t height, TaskDispatch* td = nullptr ) const;

    [[nodiscard]] uint32_t Width() const { return m_width; }
    [[nodiscard]] uint32_t Height() const { return m_height; }
    [[nodiscard]] half_float::half* Data() { return m_data; }
    [[nodiscard]] const half_float::half* Data() const { return m_data; }
    [[nodiscard]] Colorspace GetColorspace() const { return m_colorspace; }

private:
    uint32_t m_width;
    uint32_t m_height;
    half_float::half* m_data;
    Colorspace m_colorspace;
};
