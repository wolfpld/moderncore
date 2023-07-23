#ifndef __BACKENDDRM_HPP__
#define __BACKENDDRM_HPP__

#include <memory>
#include <string>
#include <vector>

#include "../Backend.hpp"

class DbusSession;
class DbusMessage;
class DrmDevice;

class BackendDrm : public Backend
{
public:
    explicit BackendDrm( VkInstance vkInstance, DbusSession& bus );
    ~BackendDrm() override;

    void VulkanInit( VlkDevice& device, VkRenderPass renderPass, const VlkSwapchain& swapchain ) override;

    void Run( const std::function<void()>& render ) override;
    void Stop() override;

    void RenderCursor( VkCommandBuffer cmdBuf, CursorLogic& cursorLogic ) override;

    operator VkSurfaceKHR() const override;

private:
    int PauseDevice( DbusMessage msg );
    int ResumeDevice( DbusMessage msg );
    int PropertiesChanged( DbusMessage msg );

    char* m_session;
    char* m_seat;

    std::string m_sessionPath;
    std::string m_seatPath;

    std::vector<std::unique_ptr<DrmDevice>> m_drmDevices;
};

#endif
