#pragma once

#include <array>
#include <memory>
#include <vector>

#include "CursorType.hpp"
#include "../util/NoCopy.hpp"
#include "../util/RobinHood.hpp"

class Bitmap;

struct CursorBitmap
{
    std::shared_ptr<Bitmap> bitmap;
    uint32_t xhot;
    uint32_t yhot;
};

struct CursorFrame
{
    uint32_t delay;     // microseconds
    uint32_t frame;
};

struct CursorData
{
    std::vector<CursorBitmap> bitmaps;
    std::vector<CursorFrame> frames;
};

struct CursorSize
{
    std::array<CursorData, (int)CursorType::NUM> type;
};

class CursorBase
{
public:
    CursorBase() = default;
    virtual ~CursorBase() = default;

    NoCopy( CursorBase );

    [[nodiscard]] virtual uint32_t FitSize( uint32_t size ) const = 0;
    [[nodiscard]] const CursorData* Get( uint32_t size, CursorType type ) const;
    [[nodiscard]] bool Valid() const { return !m_cursor.empty(); }

protected:
    unordered_flat_map<uint32_t, CursorSize> m_cursor;
};
