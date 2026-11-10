#include "transition_scene.h"

#include "engine/ui/widgets/text/ui_text_block.h"
#include "engine/ui/widgets/ui_button.h"


#include <memory>
#include <stdexcept>
#include <utility>

void TransitionScene::on_enter(const elysia::scene::ScenePayload& payload)
{

}

void TransitionScene::on_exit()
{
    _next_route.reset();
}

void TransitionScene::on_reset()
{

}