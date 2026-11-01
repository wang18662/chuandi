#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_PlayerParry.generated.h"

/**
 * 弹反（D16 / D44）：按下即触发，窗口自动持续 0.35 秒。
 *
 * 关键实现点：
 *   1. 窗口开在 ASC 的字段里（ParryWindowStart/End），判定仍由纯函数做 → V1 可测
 *   2. tag 用 AddLooseGameplayTag 同步加，不能等 GE 下帧生效（R9：同帧会误判）
 *   3. 冷却用 GE_CD_Parry，CommitAbility 时自动施加
 */
UCLASS()
class CHUANDI_API UGA_PlayerParry : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_PlayerParry();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	/** 开窗瞬间的血量，用来区分"空放"与"失败"（07 §1.2） */
	float HealthAtWindowStart = 0.f;
	/** 窗口到期：收 tag + 清窗口字段 */
	UFUNCTION()
	void OnWindowEnd();
};
