#ifndef __BACKENDDRM_HPP__
#define __BACKENDDRM_HPP__

#include <string>
#include <vector>
#include <xf86drmMode.h>

#include "../Backend.hpp"

class DbusSession;
class DbusMessage;

struct DrmCaps
{
    uint64_t dumbBuffer;
    uint64_t vblankHighCrtc;
    uint64_t dumbPreferredDepth;
    uint64_t dumbPreferShadow;
    uint64_t prime;
    uint64_t timestampMonotonic;
    uint64_t asyncPageFlip;
    uint64_t cursorWidth;
    uint64_t cursorHeight;
    uint64_t addFB2Modifiers;
    uint64_t pageFlipTarget;
    uint64_t crtcInVblankEvent;
    uint64_t syncObj;
    uint64_t syncObjTimeline;
};

struct DrmDevice
{
    ~DrmDevice();

    int fd;
    DrmCaps caps;
    drmModeRes* res;
};

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

    std::vector<DrmDevice> m_drmDevices;
};

#endif
