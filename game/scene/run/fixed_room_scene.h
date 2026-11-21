#pragma once

#include "room_scene.h"

class FixedRoomScene : public RoomScene
{
protected:
    RoomBuildResult build_room(const RoomBuildConfig& config) override;
};
