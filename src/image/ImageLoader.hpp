#pragma once

#include <memory>

#include "util/NoCopy.hpp"

class Bitmap;
class VectorImage;

class ImageLoader
{
public:
    ImageLoader() = default;
    virtual ~ImageLoader() = default;

    NoCopy( ImageLoader );

    [[nodiscard]] virtual bool IsValid() const = 0;
    [[nodiscard]] virtual std::unique_ptr<Bitmap> Load() = 0;
};

std::unique_ptr<Bitmap> LoadImage( const char* filename );
VectorImage* LoadVectorImage( const char* filename );
