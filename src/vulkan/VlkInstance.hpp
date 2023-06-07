#ifndef __VLKINSTANCE_HPP__
#define __VLKINSTANCE_HPP__

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

class VlkInstance
{
public:
    VlkInstance();
    ~VlkInstance();

    [[nodiscard]] std::vector<VkPhysicalDevice> QueryPhysicalDevices() const;

    operator VkInstance() const { return m_instance; }

private:
    VkInstance m_instance;
    VkDebugUtilsMessengerEXT m_debugMessenger;
};

#endif
