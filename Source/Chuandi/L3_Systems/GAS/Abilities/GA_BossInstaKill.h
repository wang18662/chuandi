#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_BossInstaKill.generated.h"

/**
 * 全屏秒杀（10-29 用户需求）。
 *
 * 10s 前摇：屏幕顶部醒目倒计时（HUD 读 Boss 的预警状态），Boss 原地蓄力。
 * 结算瞬间：
 *   - 玩家站在**碎石柱**上 → 柱子替他挡下，柱子被消耗掉，玩家活着；
 *   - 不在柱子上 → 直接秒杀（走统一伤害入口，死亡/结算流程照常）。
 *
 * 为什么用"伤害 9999"而不是直接 SetHealth(0)：
 * 所有死亡都必须经过 GE → PostGameplayEffectExecute → OnDeath 这条链，
 * 否则结算面板、GM 的失败判定都会漏。
 */
UCLASS()
class CHUANDI_API UGA_BossInstaKill : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_BossInstaKill();

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
	void OnResolve();
};
