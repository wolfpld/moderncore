#pragma once

#include "ImageLoader.hpp"
#include "util/NoCopy.hpp"
#include "util/FileWrapper.hpp"

class Bitmap;

class PngLoader : public ImageLoader
{
public:
    explicit PngLoader( FileWrapper& file );

    NoCopy( PngLoader );

    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] std::unique_ptr<Bitmap> Load() override;

private:
    FileWrapper& m_file;
    bool m_valid;
};
