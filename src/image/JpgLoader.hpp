#pragma once

#include <memory>
#include <stdint.h>

#include "ImageLoader.hpp"
#include "util/NoCopy.hpp"

class Bitmap;
class FileWrapper;

class JpgLoader : public ImageLoader
{
public:
    explicit JpgLoader( std::shared_ptr<FileWrapper> file );
    NoCopy( JpgLoader );

    static bool IsValidSignature( const uint8_t* buf, size_t size );

    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] std::unique_ptr<Bitmap> Load() override;

private:
    int LoadOrientation();

    bool m_valid;
    std::shared_ptr<FileWrapper> m_file;
};
