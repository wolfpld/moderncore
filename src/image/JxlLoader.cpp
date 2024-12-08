#include <jxl/cms_interface.h>
#include <jxl/color_encoding.h>
#include <jxl/decode.h>
#include <jxl/resizable_parallel_runner.h>
#include <lcms2.h>
#include <vector>

#include "JxlLoader.hpp"
#include "util/Bitmap.hpp"
#include "util/FileBuffer.hpp"
#include "util/Panic.hpp"

namespace
{
constexpr JxlColorEncoding srgb = {
    .color_space = JXL_COLOR_SPACE_RGB,
    .white_point = JXL_WHITE_POINT_D65,
    .primaries = JXL_PRIMARIES_SRGB,
    .transfer_function = JXL_TRANSFER_FUNCTION_SRGB,
    .rendering_intent = JXL_RENDERING_INTENT_PERCEPTUAL
};

struct CmsData
{
    std::vector<float*> srcBuf;
    std::vector<float*> dstBuf;

    cmsHPROFILE profileIn;
    cmsHPROFILE profileOut;
    cmsHTRANSFORM transform;
};

void* CmsInit( void* data, size_t num_threads, size_t pixels_per_thread, const JxlColorProfile* input_profile, const JxlColorProfile* output_profile, float intensity_target )
{
    auto cms = (CmsData*)data;

    cms->srcBuf.resize( num_threads );
    cms->dstBuf.resize( num_threads );

    for( size_t i=0; i<num_threads; i++ )
    {
        cms->srcBuf[i] = new float[pixels_per_thread * 3];
        cms->dstBuf[i] = new float[pixels_per_thread * 3];
    }

    cms->profileIn = cmsOpenProfileFromMem( input_profile->icc.data, input_profile->icc.size );
    cms->profileOut = cmsOpenProfileFromMem( output_profile->icc.data, output_profile->icc.size );
    cms->transform = cmsCreateTransform( cms->profileIn, TYPE_RGB_FLT, cms->profileOut, TYPE_RGB_FLT, INTENT_PERCEPTUAL, 0 );

    return cms;
}

float* CmsGetSrcBuffer( void* data, size_t thread )
{
    auto cms = (CmsData*)data;
    return cms->srcBuf[thread];
}

float* CmsGetDstBuffer( void* data, size_t thread )
{
    auto cms = (CmsData*)data;
    return cms->dstBuf[thread];
}

JXL_BOOL CmsRun( void* data, size_t thread, const float* input, float* output, size_t num_pixels )
{
    auto cms = (CmsData*)data;
    cmsDoTransform( cms->transform, input, output, num_pixels );
    return true;
}

void CmsDestroy( void* data )
{
    auto cms = (CmsData*)data;

    cmsDeleteTransform( cms->transform );
    cmsCloseProfile( cms->profileOut );
    cmsCloseProfile( cms->profileIn );

    for( auto& buf : cms->srcBuf ) delete[] buf;
    for( auto& buf : cms->dstBuf ) delete[] buf;
}
}

JxlLoader::JxlLoader( std::shared_ptr<FileWrapper> file )
    : ImageLoader( std::move( file ) )
{
    fseek( *m_file, 0, SEEK_SET );
    uint8_t hdr[12];
    const auto read = fread( hdr, 1, 12, *m_file );
    const auto res = JxlSignatureCheck( hdr, read );
    m_valid = res == JXL_SIG_CODESTREAM || res == JXL_SIG_CONTAINER;
}

bool JxlLoader::IsValid() const
{
    return m_valid;
}

std::unique_ptr<Bitmap> JxlLoader::Load()
{
    CheckPanic( m_valid, "Invalid JPEG XL file" );

    FileBuffer buf( m_file );

    auto runner = JxlResizableParallelRunnerCreate( nullptr );

    auto dec = JxlDecoderCreate( nullptr );
    JxlDecoderSubscribeEvents( dec, JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING | JXL_DEC_FULL_IMAGE );
    JxlDecoderSetParallelRunner( dec, JxlResizableParallelRunner, runner );

    JxlDecoderSetInput( dec, (const uint8_t*)buf.data(), buf.size() );
    JxlDecoderCloseInput( dec );

    CmsData cmsData = {};
    const JxlCmsInterface cmsInterface
    {
        .init_data = &cmsData,
        .init = CmsInit,
        .get_src_buf = CmsGetSrcBuffer,
        .get_dst_buf = CmsGetDstBuffer,
        .run = CmsRun,
        .destroy = CmsDestroy
    };
    JxlDecoderSetCms( dec, cmsInterface );

    std::unique_ptr<Bitmap> bmp;
    for(;;)
    {
        const auto res = JxlDecoderProcessInput( dec );
        if( res == JXL_DEC_ERROR || res == JXL_DEC_NEED_MORE_INPUT )
        {
            JxlDecoderDestroy( dec );
            JxlResizableParallelRunnerDestroy( runner );
            return nullptr;
        }
        if( res == JXL_DEC_SUCCESS || res == JXL_DEC_FULL_IMAGE ) break;
        if( res == JXL_DEC_BASIC_INFO )
        {
            JxlBasicInfo info;
            JxlDecoderGetBasicInfo( dec, &info );
            JxlResizableParallelRunnerSetThreads( runner, JxlResizableParallelRunnerSuggestThreads( info.xsize, info.ysize ) );

            bmp = std::make_unique<Bitmap>( info.xsize, info.ysize );

            JxlPixelFormat format = { 4, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0 };
            if( JxlDecoderSetImageOutBuffer( dec, &format, bmp->Data(), bmp->Width() * bmp->Height() * 4 ) != JXL_DEC_SUCCESS )
            {
                JxlDecoderDestroy( dec );
                JxlResizableParallelRunnerDestroy( runner );
                return nullptr;
            }
        }
        else if( res == JXL_DEC_COLOR_ENCODING )
        {
            JxlDecoderSetOutputColorProfile( dec, &srgb, nullptr, 0 );
        }
    }

    JxlDecoderDestroy( dec );
    JxlResizableParallelRunnerDestroy( runner );
    return bmp;
}
