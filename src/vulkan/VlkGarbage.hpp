#pragma once

#include <memory>
#include <vector>

#include "util/RobinHood.hpp"

class VlkBase;
class VlkFence;

class VlkGarbage
{
public:
    ~VlkGarbage();

    void Recycle( std::shared_ptr<VlkFence> fence, std::vector<std::shared_ptr<VlkBase>>&& objects );
    void Collect();

private:
    unordered_flat_map<std::shared_ptr<VlkFence>, std::vector<std::shared_ptr<VlkBase>>> m_garbage;
};
