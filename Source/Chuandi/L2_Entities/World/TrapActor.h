#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrapActor.generated.h"

class UStaticMeshComponent;

/**
 * 陷阱（D28：布置后 10s 未被触发即消失）。
 * 自己**不 Tick**——寿命与触发检测都交给 UTrapSubsystem 统一做，
 * 这样 10 个陷阱也只付一份 tick 成本。
 */
UCLASS()
class CHUANDI_API AChuandiTrapActor : public AActor
{
	GENERATED_BODY()

public:
	AChuandiTrapActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	float GetRadius() const { return Radius; }
	float GetSpawnTime() const { return SpawnTime; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TObjectPtr<UStaticMeshComponent> Disc;

	float Radius = 130.f;
	double SpawnTime = 0.0;
};
