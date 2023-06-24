#ifndef __BACKGROUND_HPP__
#define __BACKGROUND_HPP__

#include <stdint.h>
#include <vulkan/vulkan.h>

#include "../util/NoCopy.hpp"

class Background
{
public:
    Background();
    ~Background();

    NoCopy( Background );

    [[nodiscard]] const VkClearValue& GetColor() const { return m_color; }

private:
    VkClearValue m_color;
};

#endif
