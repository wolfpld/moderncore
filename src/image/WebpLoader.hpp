#pragma once

#include "ImageLoader.hpp"
#include "util/FileWrapper.hpp"
#include "util/NoCopy.hpp"

class Bitmap;

class WebpLoader : public ImageLoader
{
public:
    explicit WebpLoader( FileWrapper& file );

    NoCopy( WebpLoader );

    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] std::unique_ptr<Bitmap> Load() override;

private:
    FileWrapper& m_file;
    bool m_valid;
};
