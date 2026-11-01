#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BreakMeterComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBreakMeterChanged, float /*Current*/, float /*Max*/);

/**
 * 破防条（07 §1.8）—— 全 Demo 唯一的"进度可视化"。
 *
 * 三条来源：弹反 +25 / 普攻 +1 / 陷阱 +15。满 100 → 破防 3s、受伤 ×15、全局慢动作。
 * 为什么不做成 Attribute：它是"进度"不是"属性"，没有伤害要读它，
 * 放组件里可以直接广播给 UI，也不用为它写 ExecCalc。
 */
UCLASS(ClassGroup = (Chuandi), meta = (BlueprintSpawnableComponent))
class CHUANDI_API UBreakMeterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBreakMeterComponent();

	virtual void BeginPlay() override;

	/** 增加破防条；满了立即触发破防并清零 */
	void AddGauge(float Amount);

	float GetMeter() const { return Meter; }
	float GetMaxMeter() const { return MaxMeter; }
	float GetNormalized() const { return MaxMeter > 0.f ? Meter / MaxMeter : 0.f; }

	/** 每次变化都广播（UI 用） */
	FOnBreakMeterChanged OnMeterChanged;

protected:
	void TriggerBreak();

	float Meter = 0.f;
	float MaxMeter = 100.f;
};
