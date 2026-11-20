#include "Abilities/VELRATHGameplayTags.h"

namespace VelrathGameplayTags
{
UE_DEFINE_GAMEPLAY_TAG(Input_Move, "Input.Move");
UE_DEFINE_GAMEPLAY_TAG(Input_Look, "Input.Look");
UE_DEFINE_GAMEPLAY_TAG(Input_Ability_Attack_Light, "Input.Ability.Attack.Light");
UE_DEFINE_GAMEPLAY_TAG(Input_Ability_Attack_Heavy, "Input.Ability.Attack.Heavy");
UE_DEFINE_GAMEPLAY_TAG(Input_Ability_Block, "Input.Ability.Block");
UE_DEFINE_GAMEPLAY_TAG(Input_Ability_Parry, "Input.Ability.Parry");
UE_DEFINE_GAMEPLAY_TAG(Input_Ability_Dodge, "Input.Ability.Dodge");
UE_DEFINE_GAMEPLAY_TAG(Input_Ability_Echo_Cast, "Input.Ability.Echo.Cast");
UE_DEFINE_GAMEPLAY_TAG(Input_Ability_Echo_Disrupt, "Input.Ability.Echo.Disrupt");

UE_DEFINE_GAMEPLAY_TAG(State_Combat_Locomotion, "State.Combat.Locomotion");
UE_DEFINE_GAMEPLAY_TAG(State_Combat_LightAttack, "State.Combat.LightAttack");
UE_DEFINE_GAMEPLAY_TAG(State_Combat_HeavyAttack, "State.Combat.HeavyAttack");
UE_DEFINE_GAMEPLAY_TAG(State_Combat_Block, "State.Combat.Block");
UE_DEFINE_GAMEPLAY_TAG(State_Combat_Parry, "State.Combat.Parry");
UE_DEFINE_GAMEPLAY_TAG(State_Combat_Dodge, "State.Combat.Dodge");
UE_DEFINE_GAMEPLAY_TAG(State_Combat_AbilityCast, "State.Combat.AbilityCast");
UE_DEFINE_GAMEPLAY_TAG(State_Combat_EchoCast, "State.Combat.EchoCast");
UE_DEFINE_GAMEPLAY_TAG(State_Control_Stagger, "State.Control.Stagger");
UE_DEFINE_GAMEPLAY_TAG(State_Control_GuardBreak, "State.Control.GuardBreak");
UE_DEFINE_GAMEPLAY_TAG(State_Life_Dead, "State.Life.Dead");
UE_DEFINE_GAMEPLAY_TAG(State_Life_Respawn, "State.Life.Respawn");
UE_DEFINE_GAMEPLAY_TAG(State_Echo_Forming, "State.Echo.Forming");
UE_DEFINE_GAMEPLAY_TAG(State_Echo_Active, "State.Echo.Active");
UE_DEFINE_GAMEPLAY_TAG(State_Echo_Expired, "State.Echo.Expired");
UE_DEFINE_GAMEPLAY_TAG(State_Echo_Destroyed, "State.Echo.Destroyed");

UE_DEFINE_GAMEPLAY_TAG(Ability_Attack_Light, "Ability.Attack.Light");
UE_DEFINE_GAMEPLAY_TAG(Ability_Attack_Heavy, "Ability.Attack.Heavy");
UE_DEFINE_GAMEPLAY_TAG(Ability_Block, "Ability.Block");
UE_DEFINE_GAMEPLAY_TAG(Ability_Parry, "Ability.Parry");
UE_DEFINE_GAMEPLAY_TAG(Ability_Dodge, "Ability.Dodge");
UE_DEFINE_GAMEPLAY_TAG(Ability_Echo_Cast, "Ability.Echo.Cast");
UE_DEFINE_GAMEPLAY_TAG(Ability_Echo_Disrupt, "Ability.Echo.Disrupt");

UE_DEFINE_GAMEPLAY_TAG(Cooldown_Parry, "Cooldown.Parry");
UE_DEFINE_GAMEPLAY_TAG(Cooldown_Dodge, "Cooldown.Dodge");
UE_DEFINE_GAMEPLAY_TAG(Cooldown_Echo, "Cooldown.Echo");

UE_DEFINE_GAMEPLAY_TAG(Effect_Damage, "Effect.Damage");
UE_DEFINE_GAMEPLAY_TAG(Effect_StaminaCost, "Effect.StaminaCost");
UE_DEFINE_GAMEPLAY_TAG(Effect_StaminaRegen, "Effect.StaminaRegen");
UE_DEFINE_GAMEPLAY_TAG(Effect_Slow, "Effect.Slow");
UE_DEFINE_GAMEPLAY_TAG(Effect_Stagger, "Effect.Stagger");

UE_DEFINE_GAMEPLAY_TAG(Event_Attack_Hit, "Event.Attack.Hit");
UE_DEFINE_GAMEPLAY_TAG(Event_Attack_Parried, "Event.Attack.Parried");
UE_DEFINE_GAMEPLAY_TAG(Event_Echo_Spawned, "Event.Echo.Spawned");
UE_DEFINE_GAMEPLAY_TAG(Event_Echo_Expired, "Event.Echo.Expired");
}
