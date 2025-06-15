#pragma once

#include <memory>
#include <pugixml.hpp>
#include <stdint.h>

#include "ImageLoader.hpp"
#include "util/NoCopy.hpp"

class Bitmap;
class FileWrapper;
struct jpeg_decompress_struct;

class JpgLoader : public ImageLoader
{
public:
    explicit JpgLoader( std::shared_ptr<FileWrapper> file );
    ~JpgLoader() override;
    NoCopy( JpgLoader );

    static bool IsValidSignature( const uint8_t* buf, size_t size );

    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] std::unique_ptr<Bitmap> Load() override;

private:
    [[nodiscard]] bool Open();

    int LoadOrientation();
    std::unique_ptr<pugi::xml_document> LoadXmp( jpeg_decompress_struct* cinfo );

    bool m_valid;
    std::shared_ptr<FileWrapper> m_file;

    jpeg_decompress_struct* m_cinfo;
    uint8_t* m_iccData;
    int m_orientation;
    int m_gainMapOffset;
};
