#ifndef __JXLLOADER_HPP__
#define __JXLLOADER_HPP__

#include "../util/NoCopy.hpp"
#include "../util/FileWrapper.hpp"

class Bitmap;

class JxlLoader
{
public:
    explicit JxlLoader( FileWrapper& file );

    NoCopy( JxlLoader );

    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] Bitmap* Load();

private:
    FileWrapper& m_file;
    bool m_valid;
};

#endif
