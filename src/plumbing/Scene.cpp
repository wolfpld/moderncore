#include <time.h>

#include "Scene.hpp"
#include "../util/Panic.hpp"

extern "C" {
    /* wlr_scene.h header uses C99 features not recognized by a C++ compiler */
    struct wlr_scene *wlr_scene_create(void);
    struct wlr_scene_output;
    struct wlr_scene_output *wlr_scene_get_scene_output(struct wlr_scene *scene, struct wlr_output *output);
    bool wlr_scene_output_commit(struct wlr_scene_output *scene_output);
    void wlr_scene_output_send_frame_done(struct wlr_scene_output *scene_output, struct timespec *now);
    struct wlr_scene_tree;
    struct wlr_scene_rect;
    struct wlr_scene_rect *wlr_scene_rect_create(struct wlr_scene_tree *parent, int width, int height, const float color[]);
};

Scene::Scene()
    : m_scene( wlr_scene_create() )
{
    CheckPanic( m_scene, "Failed to create wlr_scene!" );

    const float color[4] = { 0.25, 0.25, 0.25, 1 };
    wlr_scene_rect_create( (wlr_scene_tree*)m_scene, 8192, 8192, color );
}

void Scene::Render( wlr_output* output )
{
    auto sceneOutput = wlr_scene_get_scene_output( m_scene, output );
    wlr_scene_output_commit( sceneOutput );

    struct timespec now;
    clock_gettime( CLOCK_MONOTONIC, &now );
    wlr_scene_output_send_frame_done( sceneOutput, &now );
}
