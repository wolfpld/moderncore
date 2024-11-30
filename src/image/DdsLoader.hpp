#pragma once

#include <stdint.h>

#include "ImageLoader.hpp"
#include "util/FileWrapper.hpp"
#include "util/NoCopy.hpp"

class Bitmap;

class DdsLoader : public ImageLoader
{
public:
    explicit DdsLoader( FileWrapper& file );

    NoCopy( DdsLoader );

    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] Bitmap* Load() override;

private:
    FileWrapper& m_file;
    bool m_valid;

    uint32_t m_format;
    uint32_t m_offset;
};
