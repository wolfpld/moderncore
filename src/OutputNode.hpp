#ifndef __OUTPUTNODE_HPP__
#define __OUTPUTNODE_HPP__

#include "Listener.hpp"

extern "C" {
    struct wlr_output;
};

class Scene;

class OutputNode
{
public:
    explicit OutputNode( wlr_output* output, Scene& scene );

private:
    void Frame();
    void Destroy();

    Scene& m_scene;

    wlr_output* m_output;

    Listener<void> m_frame;
    Listener<void> m_destroy;
};

#endif
