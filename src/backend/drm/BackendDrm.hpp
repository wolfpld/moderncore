#pragma once

#include <memory>
#include <string>
#include <vector>

#include "backend/Backend.hpp"
#include "util/NoCopy.hpp"

class DbusMessage;
class DrmDevice;

class BackendDrm : public Backend
{
public:
    explicit BackendDrm();
    ~BackendDrm() override;

    NoCopy( BackendDrm );

    void VulkanInit() override;

    void Run( const std::function<void()>& render ) override;
    void Stop() override;

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
