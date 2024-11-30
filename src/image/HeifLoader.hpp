#pragma once

#include "ImageLoader.hpp"
#include "util/FileWrapper.hpp"
#include "util/NoCopy.hpp"

class Bitmap;

class HeifLoader : public ImageLoader
{
public:
    explicit HeifLoader( FileWrapper& file );

    NoCopy( HeifLoader );

    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] Bitmap* Load() override;

private:
    FileWrapper& m_file;
    bool m_valid;
};
