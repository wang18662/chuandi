#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrapIndicatorActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * 布阱落点指示器（07 §2.1/§2.5）。
 * 长按期间跟随准星，绿色=可放置（苔绿 #8FA84A），红色=不可放置。
 * 零资产：圆柱压扁 + 动态材质改色，和预警圈同一套做法。
 */
UCLASS()
class CHUANDI_API ATrapIndicatorActor : public AActor
{
	GENERATED_BODY()

public:
	ATrapIndicatorActor();

	/** 每帧由玩家调用：更新位置与合法性 */
	void SetTarget(const FVector& Location, bool bValid, float Radius);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TObjectPtr<UStaticMeshComponent> Disc;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	bool bLastValid = true;
};
