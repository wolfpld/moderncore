#pragma once

#include <jxl/codestream_header.h>
#include <jxl/decode.h>
#include <memory>
#include <stdint.h>

#include "ImageLoader.hpp"
#include "util/NoCopy.hpp"

class Bitmap;
class BitmapHdr;
class FileBuffer;
class FileWrapper;

class JxlLoader : public ImageLoader
{
public:
    explicit JxlLoader( std::shared_ptr<FileWrapper> file );
    ~JxlLoader() override;
    NoCopy( JxlLoader );

    static bool IsValidSignature( const uint8_t* buf, size_t size );

    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] bool IsHdr() override;
    [[nodiscard]] bool PreferHdr() override;

    [[nodiscard]] std::unique_ptr<Bitmap> Load() override;
    [[nodiscard]] std::unique_ptr<BitmapHdr> LoadHdr( Colorspace colorspace ) override;

private:
    bool Open();

    bool m_valid;
    std::shared_ptr<FileWrapper> m_file;
    std::unique_ptr<FileBuffer> m_buf;

    void* m_runner;
    JxlDecoder* m_dec;
    JxlBasicInfo m_info;
};
