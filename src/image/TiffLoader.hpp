#pragma once

#include "ImageLoader.hpp"
#include "util/FileWrapper.hpp"
#include "util/NoCopy.hpp"

class Bitmap;
struct tiff;

class TiffLoader : public ImageLoader
{
public:
    explicit TiffLoader( FileWrapper& file );
    ~TiffLoader() override;

    NoCopy( TiffLoader );

    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] Bitmap* Load() override;

private:
    struct tiff* m_tiff;
};
