#include "engine/scene/scene.h"
#include "engine/physics/contracts/physics_participant.h"
#include "engine/gameplay/collision/gameplay_collision_runtime.h"
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace elysia::gameplay::collision;
using namespace elysia::physics;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
class Box final : public elysia::core::GameObject, public PhysicsParticipant
{
public:
    explicit Box(float x) : GameObject(elysia::core::DepthLayer::Character)
    {
        set_world_rect({x,0,10,10});
        collider.shape = AabbShape{{0,0,10,10}};
        collider.response = CollisionResponse::Overlap;
    }
    BodyDefinition body_definition() const override
    {
        BodyDefinition b; b.type=BodyType::Dynamic; b.gravity_scale=0; return b;
    }
    std::span<const Collider> collider_definitions() const override { return {&collider,1}; }
    void disable() { collider.enabled=false; update_physics_collider(0,collider); }
    Collider collider;
};
class TestScene final : public elysia::scene::Scene
{
public:
    using Scene::physics_world;
    void on_enter(const elysia::scene::ScenePayload&) override {}
    void on_exit() override {}
    void reset() override {}
};
struct Recorder final : GameplayCollisionListener
{
    std::vector<HitOverlapEvent> events;
    std::function<void(const HitOverlapEvent&)> callback;
    void on_hit_overlap(const HitOverlapEvent& e) override
    {
        events.push_back(e);
        if(callback) callback(e);
    }
};
struct Fixture
{
    TestScene scene;
    GameplayCollisionRuntime runtime{scene.physics_world()};
    Box* box(float x=0) { return scene.create_and_add_object<Box>(x); }
    void step() { scene.on_update(1.0/60); }
    HitBoxBinding hit(Box& box, ActorId owner, AttackInstanceId instance)
    { return {{box.physics_collider(0),owner,teams::Player,ColliderRole::HitBox},1,instance,1}; }
    void hurt(Box& box, ActorId owner, TeamId team=teams::Enemy)
    { check(runtime.bind_collider({box.physics_collider(0),owner,team,ColliderRole::HurtBox}),"bind hurt"); }
};
void validation()
{
    Fixture f; auto* box=f.box();
    check(!f.runtime.bind_collider({box->physics_collider(0),0,teams::Enemy,ColliderRole::HurtBox}),"ordinary owner required");
    ActorCollisionRig rig; rig.team=teams::Enemy; rig.body=box->physics_collider(0);
    check(!f.runtime.bind_actor(rig),"rig owner required");
    auto hit=f.hit(*box,0,10);
    auto bad=hit; bad.instigator=0; check(!f.runtime.bind_hit_box(bad),"source id required");
    bad=hit; bad.attack_instance=0; check(!f.runtime.bind_hit_box(bad),"instance required");
    bad=hit; bad.collider.team=0; check(!f.runtime.bind_hit_box(bad),"team required");
    bad=hit; bad.attack_definition=0; check(!f.runtime.bind_hit_box(bad),"definition required");
    bad=hit; bad.collider.collider=0; check(!f.runtime.bind_hit_box(bad),"collider required");
    check(f.runtime.bind_hit_box(hit),"independent owner allowed");
}
void source_removal()
{
    Fixture f; Recorder recorder; check(f.runtime.add_listener(recorder),"listener");
    auto* source=f.box(100); ActorCollisionRig rig;
    rig.owner=1;rig.team=teams::Player;rig.body=source->physics_collider(0);
    check(f.runtime.bind_actor(rig),"source rig");
    auto* bullet=f.box();auto* melee=f.box();auto* enemy=f.box();auto* friend_box=f.box();
    f.hurt(*enemy,2);f.hurt(*friend_box,3,teams::Player);
    check(f.runtime.bind_hit_box(f.hit(*bullet,0,10)),"bullet");
    check(f.runtime.bind_hit_box(f.hit(*melee,1,11)),"melee");
    check(f.runtime.unbind_actor(1),"remove source bindings");
    check(f.scene.physics_world().contains_collider(melee->physics_collider(0)),"unbind is not physical removal");
    source->destroy();f.step();
    check(recorder.events.size()==1,"only independent hostile hit survives");
    check(recorder.events[0].hit_box.instigator==1 && recorder.events[0].hit_box.collider.owner==0
        && recorder.events[0].hurt_box.owner==2,"attribution and team retained");
}
void end_during_dispatch()
{
    // Simulate the same game-layer cancellation operation for action end, interruption and death.
    for(int reason=0;reason<3;++reason)
    {
        Fixture f; Recorder first,second;
        auto* melee=f.box();auto* target=f.box();auto* target2=f.box();
        f.hurt(*target,2);f.hurt(*target2,3);
        check(f.runtime.bind_hit_box(f.hit(*melee,1,20)),"melee registered");
        first.callback=[&](const auto&) { melee->disable();f.runtime.end_attack_instance(20); };
        check(f.runtime.add_listener(first) && f.runtime.add_listener(second),"listeners");
        f.step();f.step();
        check(first.events.size()==1 && second.events.empty(),"ending suppresses remaining listener and contact deliveries");
        check(!melee->collider.enabled,"game disabled physical hitbox");
        f.runtime.end_attack_instance(20);
    }
}
void end_instance()
{
    Fixture f;Recorder recorder;check(f.runtime.add_listener(recorder),"listener");
    auto* a=f.box();auto* b=f.box();auto* other=f.box();auto* target=f.box();f.hurt(*target,2);
    const auto ha=f.hit(*a,0,30),hb=f.hit(*b,0,30),hc=f.hit(*other,0,31);
    check(f.runtime.bind_hit_box(ha)&&f.runtime.bind_hit_box(hb)&&f.runtime.bind_hit_box(hc),"multiple hitboxes");
    f.step();check(recorder.events.size()==2,"deduplicate instance across hitboxes");
    f.runtime.end_attack_instance(30);f.runtime.end_attack_instance(30);
    check(f.runtime.bind_hit_box(ha)&&f.runtime.bind_hit_box(hb),"end removed all hitbox registrations");
    check(f.scene.physics_world().teleport_object(target->physics_handle(),{100,0}),"leave");f.step();
    check(f.scene.physics_world().teleport_object(target->physics_handle(),{0,0}),"return");f.step();
    check(recorder.events.size()==3 && recorder.events.back().hit_box.attack_instance==30,
          "ended instance dedup cleared and other instance preserved");
}
int main()
{
    try { validation();source_removal();end_during_dispatch();end_instance();std::cout<<"Collision routing tests passed\n"; }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
