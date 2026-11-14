#pragma once

#include <memory>
#include <stdint.h>

#include "ImageLoader.hpp"
#include "util/NoCopy.hpp"

class Bitmap;
class DataBuffer;
class FileWrapper;
struct png_struct_def;

class PngLoader : public ImageLoader
{
public:
    explicit PngLoader( const std::shared_ptr<FileWrapper>& file, ToneMap::Operator tonemap = ToneMap::Operator::PbrNeutral, TaskDispatch* td = nullptr );
    explicit PngLoader( std::shared_ptr<DataBuffer> buf, ToneMap::Operator tonemap = ToneMap::Operator::PbrNeutral, TaskDispatch* td = nullptr );
    NoCopy( PngLoader );

    static bool IsValidSignature( const uint8_t* buf, size_t size );

    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] bool IsHdr() override;
    [[nodiscard]] std::unique_ptr<Bitmap> Load() override;
    [[nodiscard]] std::unique_ptr<BitmapHdr> LoadHdr( Colorspace colorspace ) override;

private:
    static void ReadFromBuffer( struct png_struct_def* png, unsigned char* data, size_t length );
    void QueryHeader();
    bool HasWideGamutIcc();

    std::shared_ptr<DataBuffer> m_buf;
    size_t m_offset;
    TaskDispatch* m_td;
    ToneMap::Operator m_tonemap;
    bool m_headerParsed = false;
    bool m_iccChecked = false;
    bool m_iccWide = false;
    int m_bitDepth = 0;
    int m_cicpPrimaries = -1;
    int m_cicpTransfer = -1;
    int m_cicpRange = -1;
};
