#pragma once

#include <memory>
#include <stdint.h>

#include "ImageLoader.hpp"
#include "util/NoCopy.hpp"

class Bitmap;
class DataBuffer;
class FileWrapper;

class PngLoader : public ImageLoader
{
public:
    explicit PngLoader( const std::shared_ptr<FileWrapper>& file );
    explicit PngLoader( std::shared_ptr<DataBuffer> buf );
    NoCopy( PngLoader );

    static bool IsValidSignature( const uint8_t* buf, size_t size );

    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] std::unique_ptr<Bitmap> Load() override;

private:
    std::shared_ptr<DataBuffer> m_buf;
    size_t m_offset;
};
