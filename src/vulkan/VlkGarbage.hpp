#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "util/RobinHood.hpp"

class VlkBase;
class VlkFence;

class VlkGarbage
{
public:
    VlkGarbage();
    ~VlkGarbage();

    void Recycle( std::shared_ptr<VlkFence> fence, std::shared_ptr<VlkBase>&& object );
    void Recycle( std::shared_ptr<VlkFence> fence, std::vector<std::shared_ptr<VlkBase>>&& objects );

    void Collect();

private:
    void Reaper();
    void ReapGarbage();

    std::mutex m_lock;
    unordered_flat_map<std::shared_ptr<VlkFence>, std::vector<std::shared_ptr<VlkBase>>> m_garbage;

    std::atomic<bool> m_shutdown;
    std::thread m_reaper;
    std::condition_variable m_cv;
};
