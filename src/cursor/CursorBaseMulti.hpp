#ifndef __CURSORBASEMULTI_HPP__
#define __CURSORBASEMULTI_HPP__

#include <stdint.h>
#include <vector>

#include "CursorBase.hpp"

class CursorBaseMulti : public CursorBase
{
public:
    uint32_t FitSize( uint32_t size ) override;

protected:
    void CalcSizes();

private:
    std::vector<uint32_t> m_sizes;
};

#endif
