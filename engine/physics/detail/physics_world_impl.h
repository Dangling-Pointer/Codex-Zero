#pragma once
#include "../../core/game_object.h"
#include "../collision/contact_cache.h"
#include "../physics_world.h"
#include "physics_units.h"
#include <box2d/box2d.h>
#include <functional>
#include <map>
#include <set>
#include <unordered_map>

namespace elysia::physics
{
struct PhysicsWorld::Impl
{
    struct Shape
    {
        b2ShapeId native{};
        PhysicsObjectHandle owner{};
        CollisionTarget target{};
        Collider definition{};
    };
    struct Object
    {
        b2BodyId native{};
        elysia::core::GameObject *owner = nullptr;
        BodyDefinition definition{};
        std::vector<ColliderId> shapes;
        PhysicsPose previous{}, current{};
        bool removed = false;
    };
    struct Mapping
    {
        CollisionTarget target{};
        std::uint64_t retire_after = 0;
    };
    struct Snapshot
    {
        CollisionTarget target{};
        std::optional<OneWayCollision> one_way;
        b2AABB bounds{};
        b2AABB previous_bounds{};
        b2Vec2 velocity{};
        unsigned internal_faces = 0; // left, right, top, bottom; frozen Tile adjacency
    };
    struct Joint
    {
        b2JointId native{};
        PhysicsObjectHandle first{}, second{};
        bool removed = false;
    };
    PhysicsWorldConfig config;
    detail::PhysicsUnits units;
    b2WorldId world{};
    b2BodyId tile_body{};
    const ITileCollisionWorld *tiles = nullptr;
    std::map<std::uint64_t, Object> objects;
    std::map<ColliderId, Shape> shapes;
    std::map<TileCoordinate, Shape> tile_shapes;
    std::map<std::uint64_t, Joint> joints;
    std::unordered_map<std::uint64_t, Mapping> mapping;
    std::unordered_map<std::uint64_t, Snapshot> snapshot;
    std::set<CollisionPair> previous_blocking_contacts;
    std::set<CollisionPair> ignored;
    ContactCache cache;
    std::vector<ICollisionListener *> listeners;
    std::vector<std::function<void()>> commands;
    std::uint64_t next_object = 1, next_shape = 1, next_joint = 1, epoch = 0, dropped = 0;
    double accumulator = 0;
    bool advancing = false, pending_reset = false;
    PhysicsStepStats stats{};
    PhysicsDebugCapture capture = PhysicsDebugCapture::None;
    PhysicsDebugSnapshot debug{};
    explicit Impl(PhysicsWorldConfig);
    ~Impl();
    b2Vec2 to(elysia::core::Vector2 p) const
    {
        auto v = units.to_length(p);
        return {v.x, v.y};
    }
    elysia::core::Vector2 from(b2Vec2 p) const
    {
        return units.from_length(elysia::core::Vector2{p.x, p.y});
    }
    Object *get(PhysicsObjectHandle h)
    {
        auto i = objects.find(h.value);
        return i == objects.end() || i->second.removed ? nullptr : &i->second;
    }
    Object *raw(PhysicsObjectHandle h)
    {
        auto i = objects.find(h.value);
        return i == objects.end() ? nullptr : &i->second;
    }
    const Object *get(PhysicsObjectHandle h) const
    {
        auto i = objects.find(h.value);
        return i == objects.end() || i->second.removed ? nullptr : &i->second;
    }
    void enqueue(std::function<void()> f)
    {
        if (advancing)
            commands.push_back(std::move(f));
        else
            f();
    }
    void flush();
    void clear();
    void create_shape(Shape &, b2BodyId);
    void retire(Shape &);
    void mass(Object &);
    bool bullet(const Object &) const;
    void transform(Object &, PhysicsPose, TeleportVelocityMode);
    void destroy_object(std::uint64_t);
    void build_tiles(TileCoordinate, TileCoordinate);
    void clear_tiles();
    void prepare_snapshot();
    static bool pre_solve(b2ShapeId, b2ShapeId, b2Manifold *, void *);
    void collect(std::vector<CollisionContact> &);
    void capture_debug();
    void query(const b2ShapeProxy &, b2Vec2 translation, const CollisionFilter &, bool sweep,
               bool ray, std::vector<CollisionQueryHit> &) const;
    std::optional<CollisionTarget> target(b2ShapeId id) const
    {
        auto it = mapping.find(b2StoreShapeId(id));
        return it == mapping.end() ? std::nullopt : std::optional(it->second.target);
    }
    static bool valid(const Collider &);
};
} // namespace elysia::physics
