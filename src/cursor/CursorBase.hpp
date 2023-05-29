#ifndef __CURSORBASE_HPP__
#define __CURSORBASE_HPP__

#include <array>
#include <memory>
#include <vector>

#include "CursorType.hpp"
#include "../util/RobinHood.hpp"

class Bitmap;

struct CursorBitmap
{
    std::shared_ptr<Bitmap> bitmap;
    uint32_t xhot;
    uint32_t yhot;
    uint32_t delay;
};

struct CursorSize
{
    std::array<std::vector<CursorBitmap>, (int)CursorType::NUM> type;
};

class CursorBase
{
public:
    virtual ~CursorBase() = default;

    [[nodiscard]] uint32_t FitSize( uint32_t size );
    [[nodiscard]] const std::vector<CursorBitmap>* Get( uint32_t size, CursorType type );
    [[nodiscard]] bool Valid() const { return !m_cursor.empty(); }

protected:
    void CalcSizes();

    unordered_flat_map<uint32_t, CursorSize> m_cursor;

private:
    std::vector<uint32_t> m_sizes;
};

#endif
