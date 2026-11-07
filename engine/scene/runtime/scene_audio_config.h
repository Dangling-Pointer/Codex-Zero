#pragma once

namespace elysia::scene
{
struct SceneAudioConfig
{
    // At least 0.25 seconds; failed loop acquisition never retries every frame.
    double loop_retry_interval_seconds = 0.25;
};
}
