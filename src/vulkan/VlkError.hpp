#ifndef __VLKERROR_HPP__
#define __VLKERROR_HPP__

#include <vulkan/vulkan.h>

#include "../util/Logs.hpp"

namespace impl {
    const char* TranslateVulkanResult( VkResult res );
}

#define VkVerify( res ) \
    { \
        if( (res) != VK_SUCCESS ) \
        { \
            auto str = impl::TranslateVulkanResult( res ); \
            if( str ) \
            { \
                mclog( LogLevel::Fatal, "VkVerify fail: %i (%s)", (res), str ); \
            } \
            else \
            { \
                mclog( LogLevel::Fatal, "VkVerify fail: %i", (res) ); \
            } \
        } \
    }

#endif
