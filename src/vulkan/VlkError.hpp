#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#include "util/Logs.hpp"

#define VkVerify( res ) \
    { \
        if( (res) != VK_SUCCESS ) \
        { \
            mclog( LogLevel::Fatal, "VkVerify fail: %i (%s)", (res), string_VkResult( res ) ); \
            abort(); \
        } \
    }
