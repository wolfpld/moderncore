#pragma once

#include <stdint.h>
#include <vector>

#include "CursorBase.hpp"

class CursorBaseMulti : public CursorBase
{
public:
    [[nodiscard]] uint32_t FitSize( uint32_t size ) const override;

    NoCopy( CursorBaseMulti );

protected:
    CursorBaseMulti() = default;

    void CalcSizes();

private:
    std::vector<uint32_t> m_sizes;
};
