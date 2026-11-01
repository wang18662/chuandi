#include "L2_Entities/World/TrapActor.h"

#include "L1_Framework/ChuandiTuningData.h"
#include "L3_Systems/TrapSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AChuandiTrapActor::AChuandiTrapActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Disc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Disc"));
	RootComponent = Disc;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderAsset.Succeeded())
	{
		Disc->SetStaticMesh(CylinderAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatAsset(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MatAsset.Succeeded())
	{
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> TrapMI(
			TEXT("/Game/Chuandi/Materials/MI_TrapIdle.MI_TrapIdle"));
		Disc->SetMaterial(0, TrapMI.Succeeded() ? TrapMI.Object : MatAsset.Object);

		// 04 §2.1：陷阱待触发 = 苔绿 #8FA84A
		if (UMaterialInstanceDynamic* MID = Disc->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.56f, 0.66f, 0.29f, 1.f));
		}
	}

	Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AChuandiTrapActor::BeginPlay()
{
	Super::BeginPlay();

	// 半径从 TuningData 读（之前这里是硬编码 130，导致 TrapTriggerRadius 成了死配置）
	if (const UChuandiTuningData* Tuning = UChuandiTuningData::Get())
	{
		Radius = Tuning->TrapTriggerRadius;
	}
	const float ScaleXY = Radius / 50.f;
	Disc->SetRelativeScale3D(FVector(ScaleXY, ScaleXY, 0.02f));

	SpawnTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	if (UWorld* World = GetWorld())
	{
		if (UTrapSubsystem* Subsystem = World->GetSubsystem<UTrapSubsystem>())
		{
			Subsystem->RegisterTrap(this);
		}
	}
}

void AChuandiTrapActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UTrapSubsystem* Subsystem = World->GetSubsystem<UTrapSubsystem>())
		{
			Subsystem->UnregisterTrap(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}
