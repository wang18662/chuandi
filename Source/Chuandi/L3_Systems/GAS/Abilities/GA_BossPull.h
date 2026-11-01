#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_BossPull.generated.h"

class ATelegraphActor;

/**
 * 牵引（10-29 用户需求：把玩家拉过来，强迫玩家弹反）。
 *
 * 三段：
 *   ① 锁定 0.8s —— 玩家脚下亮一个紫圈（"你被锁定了"，这段时间还能走位但跑不掉）
 *   ② 拉拽 1.2s —— 玩家被以 7m/s 拖向 Boss（玩家 4m/s 反向跑只能抵消一部分）
 *   ③ 横扫 0.5s —— Boss 身前红圈，**这一击可弹反**：窗口内按 Q = 成功（破防 +25），
 *                  没弹就吃 20 伤。这是本技能存在的唯一目的：逼玩家在"被拉到脸上"时按弹反。
 *
 * 为什么拉到 3m 就停：不把玩家塞进 Boss 碰撞体（否则会被挤飞/卡住）。
 * 冷却由状态机持有（NextPullTime），技能本身只管流程与判定。
 */
UCLASS()
class CHUANDI_API UGA_BossPull : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_BossPull();

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
	void OnLockFinished();

	UFUNCTION()
	void OnSweepFinished();

	/** 拉拽循环（20ms 一次）：把玩家沿地面拖向 Boss */
	void TickPull();

	/** 拉拽结束 → 进横扫前摇 */
	void StartSweep();

	UPROPERTY(Transient)
	TObjectPtr<ATelegraphActor> Telegraph;

	FTimerHandle PullTimer;

	/** 已经拉了多久（秒） */
	float PullElapsed = 0.f;

	/** 防止 TickPull 在收尾阶段重复触发 StartSweep */
	bool bSweepStarted = false;
};
