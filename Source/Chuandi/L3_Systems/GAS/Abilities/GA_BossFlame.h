#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_BossFlame.generated.h"

class AChuandiFlameZoneActor;

/**
 * 火焰（10-29 用户需求：火焰伤害逼玩家走位）。
 *
 * 形态：沿 **Boss 朝向** 从身前铺一条长条（默认 14m × 2.4m），
 * 先在原地暗红闪烁 1.0s（形状即预告），然后燃烧 4s，条内每 0.5s 掉 3 血。
 *
 * 为什么是长条而不是圆片：
 *   圆片"挪一步"就躲掉了，没有走位；长条只封 Boss 正前方，
 *   玩家必须**横向**跑出宽度（约 1.6m）才安全，才叫被逼走位。
 *
 * 和陨石的分工：陨石逼玩家"跑进去"（主动找圈），火焰逼玩家"横着跑开"（离开原位）。
 */
UCLASS()
class CHUANDI_API UGA_BossFlame : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_BossFlame();

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
	void OnFlameLanded();

	/** 这条火自己管"预警 → 燃烧 → 销毁"，Ability 只负责把它铺出来 */
	UPROPERTY(Transient)
	TObjectPtr<AChuandiFlameZoneActor> Zone;
};
