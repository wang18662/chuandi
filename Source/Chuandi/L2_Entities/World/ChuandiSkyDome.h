#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChuandiSkyDome.generated.h"

class UStaticMeshComponent;

/**
 * 跟随相机的天空球（11-01）。
 *
 * 为什么不用关卡里那个巨大的 SM_SkySphere：
 * 它半径约 16km，超出相机远裁剪面（默认 1km），整个被裁掉 → 天空全黑。
 * 这个 Actor 半径 800m，且每帧把中心贴到相机上，永远在裁剪面内。
 */
UCLASS()
class CHUANDI_API AChuandiSkyDome : public AActor
{
	GENERATED_BODY()

public:
	AChuandiSkyDome();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, Category = "Sky")
	TObjectPtr<UStaticMeshComponent> Dome;
};
