#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoverActor.generated.h"

class UStaticMeshComponent;

/**
 * 一次性掩体（D7）。
 * 由 Boss 的陨石砸出来，挡一发声波即碎——所以不存在"无限躲掩体"。
 *
 * 碰撞同时挡 ECC_Visibility：所以它既是物理掩体，也真的会切断 Boss 的视线，
 * 感知系统不需要为它写任何特判。
 */
UCLASS()
class CHUANDI_API AChuandiCoverActor : public AActor
{
	GENERATED_BODY()

public:
	AChuandiCoverActor();

	virtual void BeginPlay() override;

	/** 被声波打碎 */
	void BreakCover();

	/** 设置这块碎石的体量（cm）：高度决定"能跳上去还是只能躲"，底面决定占地面积 */
	void ConfigureRubble(float InHeight, float InFootprint);

	/** 柱顶世界坐标（脚底落点）—— 交互跳上去时用它 */
	FVector GetStandTopLocation() const;

	/** 被消耗掉（挡下全屏秒杀 / 被冲撞撞碎）：与"被声波打碎"分开记日志 */
	void ConsumePillar(const TCHAR* Reason);

	/** 当前高度（cm），供落点/站立判定参考 */
	float GetRubbleHeight() const { return RubbleHeight; }

	bool IsIntact() const { return bIntact; }

	double GetSpawnTime() const { return SpawnTime; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TObjectPtr<UStaticMeshComponent> Rock;

	bool bIntact = true;

	/** 体量（cm） */
	float RubbleHeight = 220.f;
	float RubbleFootprint = 140.f;

	/** 用于"超出上限时销毁最旧的"（07 §4.2 场上最多 6 块） */
	double SpawnTime = 0.0;
};
