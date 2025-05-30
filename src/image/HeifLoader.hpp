#pragma once

#include <memory>
#include <stdint.h>

#include "ImageLoader.hpp"
#include "util/NoCopy.hpp"

class Bitmap;
class BitmapHdr;
class FileBuffer;
class FileWrapper;
class TaskDispatch;

struct heif_context;
struct heif_image;
struct heif_image_handle;
struct heif_color_profile_nclx;

class HeifLoader : public ImageLoader
{
    enum class Conversion
    {
        GBR,
        BT601,
        BT709,
        BT2020
    };

public:
    explicit HeifLoader( std::shared_ptr<FileWrapper> file, ToneMap::Operator tonemap, TaskDispatch* td );
    ~HeifLoader() override;
    NoCopy( HeifLoader );

    static bool IsValidSignature( const uint8_t* buf, size_t size );

    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] bool IsHdr() override;

    [[nodiscard]] std::unique_ptr<Bitmap> Load() override;
    [[nodiscard]] std::unique_ptr<BitmapHdr> LoadHdr( Colorspace colorspace ) override;

private:
    [[nodiscard]] bool Open();

    [[nodiscard]] bool SetupDecode( bool hdr, Colorspace colorspace );

    void LoadYCbCr( float* ptr, size_t sz, size_t offset );
    void ConvertYCbCrToRGB( float* ptr, size_t sz );
    void ApplyTransfer( float* ptr, size_t sz, size_t offset );

    [[nodiscard]] bool GetGainMapHeadroom( heif_image_handle* handle );

    bool m_valid;
    ToneMap::Operator m_tonemap;
    std::shared_ptr<FileWrapper> m_file;
    std::unique_ptr<FileBuffer> m_buf;

    heif_context* m_ctx;
    heif_image_handle* m_handle;
    heif_image_handle* m_handleGainMap;
    heif_image* m_image;
    heif_color_profile_nclx* m_nclx;

    int m_width, m_height;
    int m_stride, m_bpp;
    float m_bppDiv;
    float m_gainMapHeadroom;

    Conversion m_matrix;
    Colorspace m_colorspace;

    size_t m_iccSize;
    char* m_iccData;

    float* m_gainMap;

    const uint8_t* m_planeY;
    const uint8_t* m_planeCb;
    const uint8_t* m_planeCr;
    const uint8_t* m_planeA;

    void* m_profileIn;
    void* m_profileOut;
    void* m_transform;

    TaskDispatch* m_td;
};
