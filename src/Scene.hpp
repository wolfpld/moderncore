#ifndef __SCENE_HPP__
#define __SCENE_HPP__

extern "C" {
    struct wlr_scene;
};

class Scene
{
public:
    Scene();

    operator wlr_scene* () const { return m_scene; }

private:
    struct wlr_scene* m_scene;
};

#endif
