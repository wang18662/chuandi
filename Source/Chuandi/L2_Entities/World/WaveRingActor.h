#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WaveRingActor.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * 声波波体（04 §1：声波用圆环表达"波"，与实体单位区分）。
 * 零资产：32 段 Cube 拼成单位圆环，然后整体缩放 → 看起来就是一圈向外扩的环。
 * 扩张速度 = 07 §3.3 的波速 15m/s。
 */
UCLASS()
class CHUANDI_API AWaveRingActor : public AActor
{
	GENERATED_BODY()

public:
	AWaveRingActor();

	/** 从 0 扩到 MaxRadius，速度 CmPerSecond */
	void Init(float MaxRadius, float CmPerSecond, const FLinearColor& Color);

	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TObjectPtr<UInstancedStaticMeshComponent> Segments;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	/** 单位环半径（cm）：建环时用 100，之后靠 Actor Scale 放大 */
	static constexpr float UnitRadius = 100.f;

	float MaxRadius = 600.f;
	float Speed = 1500.f;
	float Elapsed = 0.f;
};
