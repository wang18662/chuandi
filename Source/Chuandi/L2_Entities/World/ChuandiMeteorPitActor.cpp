#include "L2_Entities/World/ChuandiMeteorPitActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AChuandiMeteorPitActor::AChuandiMeteorPitActor()
{
	PrimaryActorTick.bCanEverTick = false;

	PitMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PitMesh"));
	RootComponent = PitMesh;

	// 半径 2m / 深 0.3m：圆柱压扁，稍微沉到地面以下
	PitMesh->SetRelativeScale3D(FVector(4.f, 4.f, 0.06f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderAsset.Succeeded())
	{
		PitMesh->SetStaticMesh(CylinderAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatAsset(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MatAsset.Succeeded())
	{
		PitMesh->SetMaterial(0, MatAsset.Object);
		if (UMaterialInstanceDynamic* MID = PitMesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			// 五层明度里最浓的一档
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.086f, 0.086f, 0.078f, 1.f));
		}
	}

	// 不挡路：坑只是视觉 + 减速判定
	PitMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PitMesh->SetCastShadow(false);
}

void AChuandiMeteorPitActor::BeginPlay()
{
	Super::BeginPlay();

	if (const UWorld* World = GetWorld())
	{
		SpawnTime = World->GetTimeSeconds();
	}
}

void AChuandiMeteorPitActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}
