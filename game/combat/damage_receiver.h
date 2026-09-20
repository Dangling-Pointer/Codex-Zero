class StatusEffect;

struct AttackInfo
{
    float base_damage = 0.0f;
    AttackData attack{};
};

struct AttackData
{
};

struct AttackDefinition
{
    float damage = 100.0f;
};

class CombatReceiver
{
public:
    virtual ~CombatReceiver() = default;
    virtual void receive_attack(const AttackInfo &event) = 0;
};