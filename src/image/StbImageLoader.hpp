#pragma once

#include "ImageLoader.hpp"
#include "util/NoCopy.hpp"
#include "util/FileWrapper.hpp"

class Bitmap;

class StbImageLoader : public ImageLoader
{
public:
    explicit StbImageLoader( FileWrapper& file );

    NoCopy( StbImageLoader );

    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] std::unique_ptr<Bitmap> Load() override;

private:
    FileWrapper& m_file;
    bool m_valid;
};
