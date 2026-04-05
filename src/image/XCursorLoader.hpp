#pragma once

#include "ImageLoader.hpp"
#include "util/NoCopy.hpp"

class FileWrapper;

class XCursorLoader : public ImageLoader
{
public:
    explicit XCursorLoader( std::shared_ptr<FileWrapper> file );
    NoCopy( XCursorLoader );

    static bool IsValidSignature( const uint8_t* buf, size_t size );

    [[nodiscard]] bool IsValid() const override;
    [[nodiscard]] std::unique_ptr<Bitmap> Load() override;

private:
    bool m_valid;
    std::shared_ptr<FileWrapper> m_file;

    uint32_t m_ntoc;
};
