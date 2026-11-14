#pragma once

#include <memory>
#include <stdint.h>

#include "ImageLoader.hpp"
#include "util/NoCopy.hpp"

class Bitmap;
class DataBuffer;
class FileWrapper;

class PngLoader : public ImageLoader
{
public:
    explicit PngLoader( const std::shared_ptr<FileWrapper>& file, TaskDispatch* td = nullptr );
    explicit PngLoader( std::shared_ptr<DataBuffer> buf, TaskDispatch* td = nullptr );
    NoCopy( PngLoader );

    static bool IsValidSignature( const uint8_t* buf, size_t size );

    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] std::unique_ptr<Bitmap> Load() override;

private:
    void QueryHeader();

    std::shared_ptr<DataBuffer> m_buf;
    size_t m_offset;
    TaskDispatch* m_td;
    bool m_headerParsed = false;
    int m_cicpPrimaries = -1;
    int m_cicpTransfer = -1;
    int m_cicpRange = -1;
};
