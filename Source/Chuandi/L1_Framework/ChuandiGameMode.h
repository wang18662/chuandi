#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ChuandiGameMode.generated.h"

/**
 * 关卡节奏：出生点、默认角色、HUD、胜负状态（D2：主怪血量清零 = 胜利）。
 */
UCLASS()
class CHUANDI_API AChuandiGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AChuandiGameMode();

	virtual void BeginPlay() override;

	/** 靶子/主怪在 BeginPlay 时自己注册进来，HUD 用它画顶部血条 */
	void RegisterTarget(AActor* InTarget);
	AActor* GetPrimaryTarget() const { return PrimaryTarget.Get(); }

	void NotifyTargetDied(AActor* DeadTarget);
	void NotifyPlayerDied();

	bool IsVictory() const { return bVictory; }
	bool IsDefeat() const { return bDefeat; }

	// ── 结算统计（10_玩法扩展方案：把校验点变成玩家可见数据）──
	void RegisterParryAttempt() { ++ParryAttempts; }
	void RegisterParrySuccess() { ++ParrySuccesses; }
	void RegisterBreak() { ++BreakCount; }
	void RegisterDamageTaken(float Amount) { DamageTaken += Amount; }

	int32 GetParryAttempts() const { return ParryAttempts; }
	int32 GetParrySuccesses() const { return ParrySuccesses; }
	int32 GetBreakCount() const { return BreakCount; }
	float GetDamageTaken() const { return DamageTaken; }
	/** 弹反成功率（0..1）；没有尝试时返回 -1，表示"不适用" */
	float GetParryRate() const;

	/** 从进入关卡到分出胜负的秒数 */
	float GetElapsedSeconds() const;

	/** S/A/B/C：S = 成功率 ≥80% 且未死亡；A ≥60%；B ≥40%；其余 C */
	FString GetRank() const;

protected:
	UPROPERTY()
	TWeakObjectPtr<AActor> PrimaryTarget;

	bool bVictory = false;
	bool bDefeat = false;

	int32 ParryAttempts = 0;
	int32 ParrySuccesses = 0;
	int32 BreakCount = 0;
	float DamageTaken = 0.f;

	/** 进入关卡时刻 / 分出胜负时刻（-1 = 还没结束） */
	double StartTimeSeconds = 0.0;
	double EndTimeSeconds = -1.0;
};
