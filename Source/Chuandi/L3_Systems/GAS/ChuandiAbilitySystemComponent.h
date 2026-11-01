#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "ChuandiAbilitySystemComponent.generated.h"

/**
 * ASC 子类。存在的唯一理由：把弹反窗口的"完整状态"存下来，
 * 让判定仍是纯函数（04 §4.2 / 硬约束 C2）。
 *
 * 为什么不能只靠 GameplayTag：
 *   tag 只能回答"现在开没开"，回答不了"1 毫秒前这个窗口的边界在哪"——
 *   而 V1 要求 100% 无漏判，必须能算边界。
 */
UCLASS()
class CHUANDI_API UChuandiAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	/** 开窗：调用方负责传当前世界时间（不在这里取，保持可测） */
	void OpenParryWindow(double Now, double Duration);

	/** 命中即消耗，保证"一次窗口只吃一次命中" */
	void ConsumeParryWindow();

	/** 收窗（WaitDelay 结束后调用） */
	void ResetParryWindow();

	bool IsParryWindowOpen(double Now) const;

	/** 本次窗口内是否成功弹反过（07 §1.2：成功 → 后摇 0.1s，失败 → 硬直 0.3s） */
	bool WasParryConsumed() const { return bParryConsumed; }

	/** 最近一次弹反成功 / 失败的世界时间，HUD 用它取边沿做反馈（04 §4） */
	double GetLastParrySuccessTime() const { return LastParrySuccessTime; }
	double GetLastParryFailTime() const { return LastParryFailTime; }

	double GetParryWindowStart() const { return ParryWindowStart; }
	double GetParryWindowEnd() const { return ParryWindowEnd; }
	bool IsParryConsumed() const { return bParryConsumed; }

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Chuandi|Parry")
	double ParryWindowStart = 0.0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Chuandi|Parry")
	double ParryWindowEnd = 0.0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Chuandi|Parry")
	bool bParryConsumed = false;

	double LastParrySuccessTime = -1000.0;
	double LastParryFailTime = -1000.0;
};
