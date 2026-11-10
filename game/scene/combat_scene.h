#pragma once
#include "room_scene.h"
class DungeonRoom;
class CombatScene : public RoomScene
{
protected:
    RoomBuildResult build_room(const RoomBuildConfig& config) override;
    void create_room_content() override;
private:
    // Used only during room construction; Scene owns the map.
    DungeonRoom* _map = nullptr;
};
