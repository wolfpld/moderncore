#ifndef __WLR_SCENE_HPP__
#define __WLR_SCENE_HPP__

extern "C" {
    struct wlr_output;
    struct wlr_scene;
};

class Scene
{
public:
    Scene();

    void Render( wlr_output* output );

    [[nodiscard]] operator wlr_scene* () const { return m_scene; }

private:
    wlr_scene* m_scene;
};

#endif
