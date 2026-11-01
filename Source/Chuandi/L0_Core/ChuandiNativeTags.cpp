#include "L0_Core/ChuandiNativeTags.h"

namespace ChuandiTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Frozen, "State.Frozen", "冰冻：不能行动（D33）");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Broken, "State.Broken", "破防：受到伤害 x2（D27）");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Vulnerable, "State.Vulnerable", "易伤：受到伤害提升（预留，陷阱载荷）");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Parrying, "State.Parrying", "弹反窗口开启中");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Burrowed, "State.Burrowed", "遁地：免疫冰冻（R1）");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "死亡");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Invincible, "State.Invincible", "闪避无敌帧");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Damage, "Data.Damage", "SetByCaller 伤害数值");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Parry, "Ability.Parry", "弹反");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_PlayerAttack, "Ability.PlayerAttack", "普攻");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_BossWave, "Ability.BossWave", "声波");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_BossMeteor, "Ability.BossMeteor", "陨石");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_BossBurrowStrike, "Ability.BossBurrowStrike", "遁地高伤");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_BossMelee, "Ability.BossMelee", "Boss 近战");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_PlayerDodge, "Ability.PlayerDodge", "闪避");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_PlayerSkill, "Ability.PlayerSkill", "伤害技能");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Parry, "Cooldown.Parry", "弹反冷却");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Dodge, "Cooldown.Dodge", "闪避冷却");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Attack, "Cooldown.Attack", "普攻冷却 0.6s");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Skill, "Cooldown.Skill", "伤害技能冷却 8s");
}
