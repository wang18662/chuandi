#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ChuandiCombatMath.generated.h"

/**
 * 所有"判定"都在这里，且都是纯函数：
 * 不碰 World、不取时间、不读 Actor。这样 Automation 才能直接调（硬约束 C2）。
 */
UCLASS()
class CHUANDI_API UChuandiCombatMath : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 弹反判定：命中时刻是否落在窗口内，且窗口未被消耗。
	 * HitTime 必须由调用方传入（不能用 GetWorld()->GetTimeSeconds() 在这里取），否则无法单测。
	 * 判定边界为闭区间：恰好等于 Start 或 End 都算成功。
	 */
	UFUNCTION(BlueprintPure, Category = "Chuandi|Combat")
	static bool IsParrySuccess(double WindowStart, double WindowEnd, double HitTime, bool bConsumed);

	/** 点到线段的最短距离（陷阱触发判定用，见 04 §5.2） */
	UFUNCTION(BlueprintPure, Category = "Chuandi|Combat")
	static float DistPointToSegment(const FVector& Point, const FVector& SegStart, const FVector& SegEnd);

	/** 点到线段的距离是否小于 Radius（陷阱是否被踩中） */
	UFUNCTION(BlueprintPure, Category = "Chuandi|Combat")
	static bool IsWithinSegmentRadius(const FVector& Point, const FVector& SegStart, const FVector& SegEnd, float Radius);

	/**
	 * 敌人从 SegStart 移动到 SegEnd 时，精确穿过以 Point 为心、Radius 为半径的圆的参数 t（0~1）。
	 * 返回 false 表示这一段没有穿过。用于 V3 的亚帧级测量。
	 */
	UFUNCTION(BlueprintPure, Category = "Chuandi|Combat")
	static bool FindSegmentCircleCrossT(const FVector& Point, float Radius, const FVector& SegStart, const FVector& SegEnd, float& OutT);
};
