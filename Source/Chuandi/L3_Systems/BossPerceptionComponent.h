#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BossPerceptionComponent.generated.h"

/**
 * 自写感知（04 §5.3 / A2，不用 AIPerception）。
 *
 * 20Hz 判定三条：距离 800cm、前向 60° 半角、遮挡射线无阻挡。
 * 全过 → 记 LastKnownLocation + Suspicion 累积（0.3s 确认）。
 * 失去视野后靠记忆再追 4s（V6）。
 *
 * 为什么手写：6 个状态、可打点、可单测；AIPerception 的配置在编辑器里，
 * 反而是"看不见的规则"。这条也是 A2 的落地。
 */
UCLASS(ClassGroup = (Chuandi), meta = (BlueprintSpawnableComponent))
class CHUANDI_API UBossPerceptionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossPerceptionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 连续看见超过确认时间 → 认定为"发现玩家"（V5 的打点） */
	bool IsTargetConfirmed() const { return Suspicion >= 1.f; }

	/** 失去视野超过记忆时长 → 应当放弃追击（V6 的打点） */
	bool IsTargetForgotten(double Now) const;

	bool HasLineOfSight() const { return bHasLineOfSight; }
	FVector GetLastKnownLocation() const { return LastKnownLocation; }
	float GetSuspicion() const { return Suspicion; }

	/** 调试用：视野锥是否命中 */
	bool IsPlayerInSightCone() const { return bInCone; }

	/** 受击唤醒：无视视野，直接认定"看见"了某个位置（并刷新记忆时间） */
	void ForceAwareness(const FVector& Location);

protected:
	void UpdatePerception(float DeltaTime);

	bool bHasLineOfSight = false;
	bool bInCone = false;

	/** 0..1，1 = 已确认 */
	float Suspicion = 0.f;

	FVector LastKnownLocation = FVector::ZeroVector;
	double LastSeenTime = -1000.0;

	/** 诊断节流：玩家在范围内却看不见时，2 秒打一次原因 */
	float DiagnoseAccum = 0.f;
};
