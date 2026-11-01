#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TelegraphActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class USceneComponent;

/**
 * 通用地面预警圈：D3 声波用它，D7 陨石也用它。
 * 零资产——引擎自带圆柱压扁 + 动态材质改色，随时间扩张，到点自己销毁。
 */
UCLASS()
class CHUANDI_API ATelegraphActor : public AActor
{
	GENERATED_BODY()

public:
	ATelegraphActor();

	/** 开始预警：Duration 秒内从 30% 扩张到 Radius，颜色 Color */
	void PlayWarning(float Duration, float Radius, const FLinearColor& Color);

protected:
	virtual void Tick(float DeltaSeconds) override;

	/** 根节点：**不缩放**。填充盘和边界环都挂它下面，否则父级缩放会把"固定的圈"一起放大 */
	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TObjectPtr<USceneComponent> Root;

	/** 内部填充圆盘：随进度从 0 涨到目标半径（10-28 20:21：黄环逐渐填充满） */
	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TObjectPtr<UStaticMeshComponent> Disc;

	/** 外圈环：沿目标半径摆一圈小柱，**半径固定**，只当"预告区有多大"的边界 */
	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TArray<TObjectPtr<UStaticMeshComponent>> RingSegments;

	/** 32 段：段数足够多 + 每段按弧长放大，视觉上是一条连续圆环（16 段在 4m 半径下会露出大缺口） */
	static constexpr int32 RingSegmentCount = 32;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	float Elapsed = 0.f;
	float TotalDuration = 1.f;
	float TargetRadius = 300.f;
	bool bRunning = false;

	/** 4Hz 闪烁用的基色（04 §4：陨石预警 1.5s 闪 6 次） */
	FLinearColor BaseColor = FLinearColor::White;
};
