#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_PlayerSkill.generated.h"

/**
 * 玩家伤害技能（07 §1.5 最终方案）：
 * 平时 15 伤害 / 前方 3m 锥形 / 8s 冷却；破防期 ×12 = 180。
 *
 * 设计意图是"纯数值引导"：平时放等于浪费，玩家自己会算出"该留着等破防"。
 * 所以这里**不加任何特殊规则**（不给它破防期额外加成），倍率由 ExecCalc 统一处理。
 */
UCLASS()
class CHUANDI_API UGA_PlayerSkill : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_PlayerSkill();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
