#pragma once

#include "ImageLoader.hpp"
#include "util/FileWrapper.hpp"
#include "util/NoCopy.hpp"

class Bitmap;

class JpgLoader : public ImageLoader
{
public:
    explicit JpgLoader( FileWrapper& file );

    NoCopy( JpgLoader );

    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] Bitmap* Load() override;

private:
    FileWrapper& m_file;
    bool m_valid;
};
