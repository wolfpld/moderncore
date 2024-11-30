#pragma once

#include <memory>

#include "util/NoCopy.hpp"

class Bitmap;
class FileWrapper;
class VectorImage;

class ImageLoader
{
public:
    ImageLoader( std::shared_ptr<FileWrapper>&& file ) : m_file( std::move( file ) ) {}
    virtual ~ImageLoader() = default;

    NoCopy( ImageLoader );

    [[nodiscard]] virtual bool IsValid() const = 0;
    [[nodiscard]] virtual std::unique_ptr<Bitmap> Load() = 0;

protected:
    std::shared_ptr<FileWrapper> m_file;
};

std::unique_ptr<Bitmap> LoadImage( const char* filename );
std::unique_ptr<VectorImage> LoadVectorImage( const char* filename );
