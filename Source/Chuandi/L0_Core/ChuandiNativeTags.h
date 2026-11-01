#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * 原生 GameplayTag 集中声明。
 * 好处：没有字符串拼写错误、不依赖 ini 加载顺序、编译期就有符号。
 * 这里声明的 tag 会同时出现在编辑器的 tag 选择器里。
 */
namespace ChuandiTags
{
	// 状态
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Frozen);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Broken);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Vulnerable);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Parrying);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Burrowed);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	/** 闪避无敌帧（07 §0） */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Invincible);

	// 数据
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);

	// 技能（用于日志与互斥）
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Parry);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_PlayerAttack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_BossWave);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_BossMeteor);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_BossBurrowStrike);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_BossMelee);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_PlayerDodge);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_PlayerSkill);

	// 冷却
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Parry);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Dodge);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Attack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill);
}
