#pragma once

#include "../util/NoCopy.hpp"
#include "../util/FileWrapper.hpp"

class Bitmap;

class JpgLoader
{
public:
    explicit JpgLoader( FileWrapper& file );

    NoCopy( JpgLoader );

    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] Bitmap* Load();

private:
    FileWrapper& m_file;
    bool m_valid;
};
