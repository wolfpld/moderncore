#pragma once

#include <memory>
#include <stdint.h>

#include "ImageLoader.hpp"
#include "util/NoCopy.hpp"

class Bitmap;
class FileWrapper;
struct tiff;

class TiffLoader : public ImageLoader
{
public:
    explicit TiffLoader( std::shared_ptr<FileWrapper> file );
    ~TiffLoader() override;
    NoCopy( TiffLoader );

    static bool IsValidSignature( const uint8_t* buf, size_t size );

    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] std::unique_ptr<Bitmap> Load() override;

private:
    std::shared_ptr<FileWrapper> m_file;
    struct tiff* m_tiff;
};
