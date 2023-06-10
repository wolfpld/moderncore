#ifndef __VLKERROR_HPP__
#define __VLKERROR_HPP__

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#include "../util/Logs.hpp"

#define VkVerify( res ) \
    { \
        if( (res) != VK_SUCCESS ) \
        { \
            mclog( LogLevel::Fatal, "VkVerify fail: %i (%s)", (res), string_VkResult( res ) ); \
        } \
    }

#endif
