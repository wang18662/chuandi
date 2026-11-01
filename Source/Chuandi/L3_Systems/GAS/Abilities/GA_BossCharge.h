#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_BossCharge.generated.h"

class ATelegraphActor;
class AChuandiCoverActor;

/**
 * 冲撞（10-30 改版：锁定**玩家**，不是锁柱子）。
 *
 * 流程：
 *   ① 锁定 5s —— Boss 转身盯住你，你脚下亮红圈，屏幕上方有倒计时提示
 *   ② 朝你**直线冲刺** 13m/s —— 跑速只有 4m/s，直线跑是跑不掉的
 *   ③ 解法：**跑到碎石柱后面**，让它撞上柱子
 *
 * 撞到柱子：柱子碎 + Boss 停顿 0.5s，累计 3 根 → 直接破防。
 * 撞到玩家：15 伤，冲撞停止。
 *
 * 为什么方向要在"起飞那一刻"才定：
 * 若在 5s 前就锁定方向，玩家走两步就能侧闪，柱子就不需要了。
 * 起飞瞬间取方向 = "你必须提前把柱子摆在自己和 Boss 之间"，这才是这个技能要考的东西。
 */
UCLASS()
class CHUANDI_API UGA_BossCharge : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_BossCharge();

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
	void OnWindupFinished();

	/** 冲撞推进（16ms 一拍） */
	void TickCharge();

	/** 收尾：清计时器/锁定/预警圈、通知 FSM、结束技能 */
	void FinishCharge(const TCHAR* Reason);

	/** 撞碎一根柱子：计数 + 可能触发破防。返回 true = 这一根撞满了阈值、已破防 */
	bool BreakPillar(AChuandiCoverActor* Pillar);

	UPROPERTY(Transient)
	TObjectPtr<ATelegraphActor> Telegraph;

	FTimerHandle ChargeTimer;

	/** 起飞瞬间锁定的冲刺方向（水平单位向量） */
	FVector ChargeDirection = FVector::ForwardVector;

	float ChargeElapsed = 0.f;

	bool bFinished = false;

	/** 本次冲撞已经撞碎几根（用来判断"撞满 3 根 → 开接住窗口"） */
	int32 PillarsBrokenInThisCharge = 0;
};
