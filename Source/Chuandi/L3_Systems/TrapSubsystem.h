#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TrapSubsystem.generated.h"

class AChuandiTrapActor;

/**
 * 陷阱管理器：寿命 + 触发检测都集中在这里，**以 30Hz 统一扫描**。
 *
 * 为什么不让每个陷阱自己 Tick：
 *   10 个陷阱 = 10 份 tick 开销；集中扫描只有一份，且以后加"陷阱上限"这类规则只改一处。
 *   （正式项目里更省的做法是用 Overlap 事件，但灰盒阶段距离判定更可控、更好调试。）
 */
UCLASS()
class CHUANDI_API UTrapSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	void RegisterTrap(AChuandiTrapActor* Trap);
	void UnregisterTrap(AChuandiTrapActor* Trap);

	int32 GetActiveTrapCount() const { return Traps.Num(); }

private:
	void CheckTraps();

	UPROPERTY(Transient)
	TArray<TObjectPtr<AChuandiTrapActor>> Traps;

	/** 距离上次扫描累计的时间，用来把扫描压到 30Hz */
	float TimeSinceScan = 0.f;
};
