#include "Panic.hpp"
#include "Scene.hpp"

extern "C" {
    /* wlr_scene.h header uses C99 features not recognized by a C++ compiler */
    struct wlr_scene *wlr_scene_create(void);
};

Scene::Scene()
    : m_scene( wlr_scene_create() )
{
    CheckPanic( m_scene, "Failed to create wlr_scene!" );
}
