#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_BossBurrow.generated.h"

class ATelegraphActor;

/**
 * 遁地高伤（07 §3.2/§3.3，C-4）：
 *   任意 → 遁地：血量 ≤ 60% 且未遁过（只一次）
 *   遁地 → 破防：弹反成功（唯一解：弹反 → 破防 → 强制出土）
 *
 * 时间轴：0.8s 下沉（地面裂纹）→ 1.5s 出土前摇 → 出土判定 40 伤害。
 * 遁地期间挂 State.Burrowed：不可选中、免疫冰冻（R1）。
 */
UCLASS()
class CHUANDI_API UGA_BossBurrow : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_BossBurrow();

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
	void OnSinkFinished();

	UFUNCTION()
	void OnStrike();

	UPROPERTY(Transient)
	TObjectPtr<ATelegraphActor> Telegraph;

	float CachedRadius = 400.f;
	float CachedDamage = 40.f;
};
