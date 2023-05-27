#ifndef __OUTPUTNODE_HPP__
#define __OUTPUTNODE_HPP__

#include "Listener.hpp"

extern "C" {
    struct wlr_output;
};

class Output;
class Scene;

class OutputNode
{
public:
    OutputNode( wlr_output* output, Scene& scene, Output& parent );

private:
    void Frame();
    void Destroy();

    Scene& m_scene;
    Output& m_parent;

    wlr_output* m_output;

    Listener<void> m_frame;
    Listener<void> m_destroy;
};

#endif
