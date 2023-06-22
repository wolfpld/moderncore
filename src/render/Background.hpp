#ifndef __BACKGROUND_HPP__
#define __BACKGROUND_HPP__

#include <stdint.h>
#include <vulkan/vulkan.h>

class Background
{
public:
    Background();
    ~Background();

    [[nodiscard]] const VkClearValue& GetColor() const { return m_color; }

private:
    VkClearValue m_color;
};

#endif
