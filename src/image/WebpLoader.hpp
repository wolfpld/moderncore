#ifndef __WEBPLOADER_HPP__
#define __WEBPLOADER_HPP__

#include "../util/NoCopy.hpp"
#include "../util/FileWrapper.hpp"

class Bitmap;

class WebpLoader
{
public:
    explicit WebpLoader( FileWrapper& file );

    NoCopy( WebpLoader );

    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] Bitmap* Load();

private:
    FileWrapper& m_file;
    bool m_valid;
};

#endif
