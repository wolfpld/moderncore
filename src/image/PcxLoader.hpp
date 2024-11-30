#pragma once

#include "ImageLoader.hpp"
#include "util/NoCopy.hpp"
#include "util/FileWrapper.hpp"

class Bitmap;

class PcxLoader : public ImageLoader
{
public:
    explicit PcxLoader( FileWrapper& file );

    NoCopy( PcxLoader );

    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] std::unique_ptr<Bitmap> Load() override;

private:
    FileWrapper& m_file;
    bool m_valid;
};
