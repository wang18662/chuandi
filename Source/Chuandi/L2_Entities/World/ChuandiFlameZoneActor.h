#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChuandiFlameZoneActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class USceneComponent;

/**
 * 火焰长条（10-29 用户要求：长条状、方向与 Boss 朝向一致）。
 *
 * 预告规则与陨石统一：**外框固定 = 最终伤害区**，内部填充从 Boss 一侧往前长，
 * 长满即落地。这样玩家不用读秒，看"填到哪了"就知道还剩多久。
 *
 *   ┌────────────────────────┐  ← 4 根固定边条（暗红，4Hz 闪）
 *   │██████                  │  ← 填充（沿 +X 从起点往远端长）
 *   └────────────────────────┘
 *
 * 判定用**局部坐标**（把玩家位置转进本地空间），旋转/朝向天然生效。
 */
UCLASS()
class CHUANDI_API AChuandiFlameZoneActor : public AActor
{
	GENERATED_BODY()

public:
	AChuandiFlameZoneActor();

	/** 铺条：预警 InWarningTime 秒（填充长满）→ 燃烧 InBurnTime 秒；Actor 位置 = 长条中心 */
	void BeginStrip(float InLength, float InWidth, float InWarningTime, float InBurnTime,
		float InTickInterval, float InTickDamage, AActor* InInstigator);

protected:
	virtual void Tick(float DeltaSeconds) override;

	/** 玩家是否站在长条内（局部坐标判定，忽略高度差） */
	bool IsPlayerInside() const;

	/** 所有材质实例统一改色（边框） */
	void SetFrameColor(const FLinearColor& Color);

	/** 填充单独改色：预警期填充保持暗色，只有边框在闪 */
	void SetFillColor(const FLinearColor& Color);

	/** 按进度 0~1 更新填充：从 -X 端（靠 Boss）往 +X 端长 */
	void UpdateFill(float Alpha);

	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TObjectPtr<USceneComponent> Root;

	/** 内部填充板：长度随预警进度增长，宽度=整个条宽 */
	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TObjectPtr<UStaticMeshComponent> Fill;

	/** 4 根固定边条：长边 2 根 + 端头 2 根，组成"最终伤害区"的边界 */
	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TArray<TObjectPtr<UStaticMeshComponent>> Frame;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FillMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> FrameMaterials;

	float Length = 1400.f;
	float Width = 240.f;
	float WarningTime = 1.f;
	float BurnTime = 4.f;
	float TickInterval = 0.5f;
	float TickDamage = 3.f;

	float Elapsed = 0.f;
	float TickAccum = 0.f;
	bool bBurning = false;

	/** 伤害来源（Boss），交给 CombatLibrary 结算 */
	TWeakObjectPtr<AActor> DamageInstigator;
};
