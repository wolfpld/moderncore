#ifndef __HEIFLOADER_HPP__
#define __HEIFLOADER_HPP__

#include "../util/NoCopy.hpp"
#include "../util/FileWrapper.hpp"

class Bitmap;

class HeifLoader
{
public:
    explicit HeifLoader( FileWrapper& file );

    NoCopy( HeifLoader );

    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] Bitmap* Load();

private:
    FileWrapper& m_file;
    bool m_valid;
};

#endif
