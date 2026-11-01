#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_PlayerAttack.generated.h"

/**
 * 普攻：前方扇形范围内找最近的一个带 ASC 的目标，造成伤害。
 * 数值读 TuningData（不写死），所以调伤害不用重编译。
 */
UCLASS()
class CHUANDI_API UGA_PlayerAttack : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_PlayerAttack();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
