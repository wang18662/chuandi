#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_BossMelee.generated.h"

class ATelegraphActor;

/**
 * Boss 近战（07 §3.3）：2m / 10 伤 / 前摇 0.4s / 冷却 1.2s。
 * 冷却由状态机持有（NextMeleeTime），技能本身只管表现与判定。
 * 前摇用一个小红圈做可读性替代（没有动画资源）。
 */
UCLASS()
class CHUANDI_API UGA_BossMelee : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_BossMelee();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	UFUNCTION()
	void OnMeleeHit();

	UPROPERTY(Transient)
	TObjectPtr<ATelegraphActor> Telegraph;
};
