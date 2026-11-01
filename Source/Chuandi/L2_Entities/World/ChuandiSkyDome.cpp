#include "L2_Entities/World/ChuandiSkyDome.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

AChuandiSkyDome::AChuandiSkyDome()
{
	PrimaryActorTick.bCanEverTick = true;

	Dome = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Dome"));
	RootComponent = Dome;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Dome->SetStaticMesh(SphereMesh.Object);
	}

	// 火山天空材质（由导入脚本生成）
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SkyMat(TEXT("/Game/Chuandi/Materials/M_VolcanicSky.M_VolcanicSky"));
	if (SkyMat.Succeeded())
	{
		Dome->SetMaterial(0, SkyMat.Object);
	}

	// 球半径 = 50（球体直径 100）× 1600 = 80000cm = 800m，在默认 1km 远裁剪面内
	Dome->SetRelativeScale3D(FVector(1600.f, 1600.f, 1600.f));
	Dome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Dome->SetCastShadow(false);
}

void AChuandiSkyDome::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 中心始终贴住相机：玩家在无边界场地里怎么跑，天空都不会被甩开
	if (const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (const AActor* ViewTarget = PC->GetViewTarget())
		{
			SetActorLocation(ViewTarget->GetActorLocation());
		}
	}
}
