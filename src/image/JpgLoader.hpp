#pragma once

#include <memory>
#include <pugixml.hpp>
#include <stdint.h>

#include "ImageLoader.hpp"
#include "util/NoCopy.hpp"

class Bitmap;
class FileWrapper;
class TaskDispatch;
struct jpeg_decompress_struct;

class JpgLoader : public ImageLoader
{
public:
    explicit JpgLoader( std::shared_ptr<FileWrapper> file, TaskDispatch* td );
    ~JpgLoader() override;
    NoCopy( JpgLoader );

    static bool IsValidSignature( const uint8_t* buf, size_t size );

    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] bool IsHdr() override;

    [[nodiscard]] std::unique_ptr<Bitmap> Load() override;
    [[nodiscard]] std::unique_ptr<BitmapHdr> LoadHdr( Colorspace colorspace ) override;

private:
    [[nodiscard]] bool Open();

    int LoadOrientation();
    std::unique_ptr<pugi::xml_document> LoadXmp( jpeg_decompress_struct* cinfo );
    [[nodiscard]] std::unique_ptr<Bitmap> LoadNoColorspace();

    bool m_valid;
    std::shared_ptr<FileWrapper> m_file;

    TaskDispatch* m_td;

    jpeg_decompress_struct* m_cinfo;
    unsigned int m_iccSz;
    uint8_t* m_iccData;
    bool m_cmyk;
    bool m_grayScale;
    int m_orientation;
    int m_gainMapOffset;
    bool m_isIso;
};
