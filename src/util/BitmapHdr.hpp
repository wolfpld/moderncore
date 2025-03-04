#pragma once

#include <memory>
#include <stdint.h>

#include "NoCopy.hpp"
#include "Tonemapper.hpp"

class Bitmap;
class TaskDispatch;

class BitmapHdr
{
public:
    BitmapHdr( uint32_t width, uint32_t height );
    ~BitmapHdr();
    NoCopy( BitmapHdr );

    void Resize( uint32_t width, uint32_t height, TaskDispatch* td = nullptr );
    [[nodiscard]] std::unique_ptr<BitmapHdr> ResizeNew( uint32_t width, uint32_t height, TaskDispatch* td = nullptr ) const;

    [[nodiscard]] uint32_t Width() const { return m_width; }
    [[nodiscard]] uint32_t Height() const { return m_height; }
    [[nodiscard]] float* Data() { return m_data; }
    [[nodiscard]] const float* Data() const { return m_data; }

    [[nodiscard]] std::unique_ptr<Bitmap> Tonemap( ToneMap::Operator op );

private:
    uint32_t m_width;
    uint32_t m_height;
    float* m_data;
};
