#pragma once

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
    [[nodiscard]] virtual Bitmap* Load() = 0;
};

Bitmap* LoadImage( const char* filename );
VectorImage* LoadVectorImage( const char* filename );
