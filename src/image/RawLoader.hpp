#pragma once

#include <memory>

#include "ImageLoader.hpp"
#include "util/NoCopy.hpp"

class Bitmap;
class FileBuffer;
class FileWrapper;
class LibRaw;

class RawLoader : public ImageLoader
{
public:
    explicit RawLoader( const std::shared_ptr<FileWrapper>& file );
    ~RawLoader() override;
    NoCopy( RawLoader );

    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] bool IsHdr() override { return true; }

    [[nodiscard]] std::unique_ptr<Bitmap> Load() override;
    [[nodiscard]] std::unique_ptr<BitmapHdr> LoadHdr( Colorspace colorspace ) override;

private:
    std::unique_ptr<LibRaw> m_raw;
    std::unique_ptr<FileBuffer> m_buf;

    bool m_valid;
};
