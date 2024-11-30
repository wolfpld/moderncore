#pragma once

#include <memory>

#include "ImageLoader.hpp"
#include "util/FileWrapper.hpp"
#include "util/NoCopy.hpp"

class Bitmap;
class FileBuffer;
class LibRaw;

class RawLoader : public ImageLoader
{
public:
    explicit RawLoader( FileWrapper& file );
    ~RawLoader() override;

    NoCopy( RawLoader );

    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] Bitmap* Load() override;

private:
    std::unique_ptr<LibRaw> m_raw;
    std::unique_ptr<FileBuffer> m_buf;

    bool m_valid;
};
