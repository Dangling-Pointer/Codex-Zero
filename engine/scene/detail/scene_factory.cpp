#include "scene_factory.h"

namespace elysia::scene
{
SceneFactory::~SceneFactory()
{
	destroy_all_scene();
}

void SceneFactory::clear_runtime_contexts() noexcept
{
	for (auto& [type, scene] : _scene_cache)
	{
		(void)type;
		if (scene)
			scene->clear_runtime_context();
	}
}

bool SceneFactory::destroy_all_scene()
{
	_scene_cache.clear();
	return true;
}

}
