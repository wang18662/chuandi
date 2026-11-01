#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChuandiMeteorPitActor.generated.h"

class UStaticMeshComponent;

/**
 * 陨石坑（交接 10-28 17:10）：落点中心，半径 2m / 深 0.3m / 全场上限 5 个。
 *
 * 只做两件事：
 *   1. 视觉：地面一个下沉的深色圆盘（#161614，最浓的墨）
 *   2. 机制：玩家在坑内移速 ×0.7（由 PlayerCharacter 每帧查询）
 *
 * 刻意不做：
 *   - 不参与碰撞（否则 Boss 寻路会卡，回到"卡住"那个老问题）
 *   - 不挡声波（0.3m 的坑挡不住 1.5m 高的波，挡了反而不合理）
 *   - 不影响 Boss（体型 1.8 倍，0.3m 不算地形）
 */
UCLASS()
class CHUANDI_API AChuandiMeteorPitActor : public AActor
{
	GENERATED_BODY()

public:
	AChuandiMeteorPitActor();

	/** 进入世界的时刻，超出上限时用来淘汰最旧的 */
	double GetSpawnTime() const { return SpawnTime; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TObjectPtr<UStaticMeshComponent> PitMesh;

	double SpawnTime = 0.0;
};
