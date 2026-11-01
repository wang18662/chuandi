#include "L2_Entities/World/WaveRingActor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AWaveRingActor::AWaveRingActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	Segments = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Segments"));
	RootComponent = Segments;
	Segments->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Segments->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeAsset.Succeeded())
	{
		Segments->SetStaticMesh(CubeAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatAsset(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MatAsset.Succeeded())
	{
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> WaveMI(
			TEXT("/Game/Chuandi/Materials/MI_Wave.MI_Wave"));
		Segments->SetMaterial(0, WaveMI.Succeeded() ? WaveMI.Object : MatAsset.Object);
	}
}

void AWaveRingActor::Init(float InMaxRadius, float CmPerSecond, const FLinearColor& Color)
{
	MaxRadius = FMath::Max(50.f, InMaxRadius);
	Speed = FMath::Max(100.f, CmPerSecond);
	Elapsed = 0.f;

	// 32 段拼单位环：段长按周长均分，避免重叠
	constexpr int32 SegmentCount = 32;
	const float SegmentLength = (2.f * PI * UnitRadius) / SegmentCount;

	Segments->ClearInstances();
	for (int32 i = 0; i < SegmentCount; ++i)
	{
		const float Angle = (2.f * PI * i) / SegmentCount;
		const FVector Location(FMath::Cos(Angle) * UnitRadius, FMath::Sin(Angle) * UnitRadius, 0.f);
		const FRotator Rotation(0.f, FMath::RadiansToDegrees(Angle) + 90.f, 0.f);
		// 缩放按 Cube 100cm 基准：长度=段长，厚度=8cm，高度=30cm
		const FVector Scale(SegmentLength / 100.f, 0.08f, 0.30f);

		Segments->AddInstance(FTransform(Rotation, Location, Scale));
	}

	Material = Segments->CreateAndSetMaterialInstanceDynamic(0);
	if (Material)
	{
		// 04 §2.1：声波 = 紫 #8B5CF6
		Material->SetVectorParameterValue(TEXT("Color"), Color);
		// 能量环要发光（11 表现层：声波是"看得见的能量"）
		Material->SetVectorParameterValue(TEXT("Emissive"), Color);
		Material->SetScalarParameterValue(TEXT("EmissiveStrength"), 4.f);
	}

	SetActorScale3D(FVector(0.01f, 0.01f, 1.f));
}

void AWaveRingActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Elapsed += DeltaSeconds;
	const float Radius = Speed * Elapsed;

	if (Radius >= MaxRadius)
	{
		Destroy();
		return;
	}

	const float ScaleXY = Radius / UnitRadius;
	SetActorScale3D(FVector(FMath::Max(0.01f, ScaleXY), FMath::Max(0.01f, ScaleXY), 1.f));
}
