#pragma once

#include "ImageLoader.hpp"
#include "util/FileWrapper.hpp"
#include "util/NoCopy.hpp"

class Bitmap;

class WebpLoader : public ImageLoader
{
public:
    explicit WebpLoader( std::shared_ptr<FileWrapper> file );

    NoCopy( WebpLoader );

    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] std::unique_ptr<Bitmap> Load() override;

private:
    bool m_valid;
};
