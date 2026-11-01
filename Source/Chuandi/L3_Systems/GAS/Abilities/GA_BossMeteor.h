#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_BossMeteor.generated.h"

class ATelegraphActor;

/**
 * 陨石（D7）：1.5 秒三个光圈预告 → 落点砸出碎石掩体。
 *
 * 设计意图（02 §3.5）：**掩体是 Boss 自己砸出来的**。
 * 玩家用敌人的力量保护自己 —— 同时"一次性"自动堵死了无限躲掩体。
 */
UCLASS()
class CHUANDI_API UGA_BossMeteor : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_BossMeteor();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	UFUNCTION()
	void OnMeteorLand();

	UPROPERTY(Transient)
	TArray<TObjectPtr<ATelegraphActor>> Telegraphs;

	TArray<FVector> ImpactPoints;
	float CachedRadius = 220.f;
	float CachedDamage = 12.f;
};
