#include "L2_Entities/World/TrapIndicatorActor.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ATrapIndicatorActor::ATrapIndicatorActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Disc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Disc"));
	RootComponent = Disc;
	Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderAsset.Succeeded())
	{
		Disc->SetStaticMesh(CylinderAsset.Object);
	}

	// 几何体风格要求"贴地、不挡视线"（04 §1）
	Disc->SetRelativeScale3D(FVector(3.f, 3.f, 0.02f));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatAsset(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MatAsset.Succeeded())
	{
		Disc->SetMaterial(0, MatAsset.Object);
	}
}

void ATrapIndicatorActor::SetTarget(const FVector& Location, bool bValid, float Radius)
{
	SetActorLocation(Location + FVector(0.f, 0.f, 2.f));

	const float ScaleXY = Radius / 50.f;
	Disc->SetWorldScale3D(FVector(ScaleXY, ScaleXY, 0.02f));

	if (!DynamicMaterial)
	{
		DynamicMaterial = Disc->CreateAndSetMaterialInstanceDynamic(0);
	}

	if (!DynamicMaterial)
	{
		return;
	}

	// 07 §2.5：可放置=苔绿，不可放置=红
	bLastValid = bValid;
	DynamicMaterial->SetVectorParameterValue(TEXT("Color"),
		bValid ? FLinearColor(0.56f, 0.66f, 0.29f, 1.f)      // #8FA84A 苔绿
		: FLinearColor(0.85f, 0.15f, 0.15f, 1.f));           // 不可放置红
}
