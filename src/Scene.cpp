#include <time.h>

#include "Panic.hpp"
#include "Scene.hpp"

extern "C" {
    /* wlr_scene.h header uses C99 features not recognized by a C++ compiler */
    struct wlr_scene *wlr_scene_create(void);
    struct wlr_scene_output;
    struct wlr_scene_output *wlr_scene_get_scene_output(struct wlr_scene *scene, struct wlr_output *output);
    bool wlr_scene_output_commit(struct wlr_scene_output *scene_output);
    void wlr_scene_output_send_frame_done(struct wlr_scene_output *scene_output, struct timespec *now);
};

Scene::Scene()
    : m_scene( wlr_scene_create() )
{
    CheckPanic( m_scene, "Failed to create wlr_scene!" );
}

void Scene::Render( wlr_output* output )
{
    auto sceneOutput = wlr_scene_get_scene_output( m_scene, output );
    wlr_scene_output_commit( sceneOutput );

    struct timespec now;
    clock_gettime( CLOCK_MONOTONIC, &now );
    wlr_scene_output_send_frame_done( sceneOutput, &now );
}
