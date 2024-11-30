#pragma once

#include <stdint.h>

#include "ImageLoader.hpp"
#include "util/FileWrapper.hpp"
#include "util/NoCopy.hpp"

class Bitmap;

class PvrLoader : public ImageLoader
{
public:
    explicit PvrLoader( FileWrapper& file );

    NoCopy( PvrLoader );

    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] std::unique_ptr<Bitmap> Load() override;

private:
    FileWrapper& m_file;
    bool m_valid;

    uint32_t m_format;
};
