#include "L2_Entities/World/TelegraphActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// 引擎自带圆柱：半径 50、高 100（缩放前）
	constexpr float BaseCylinderRadius = 50.f;
}

ATelegraphActor::ATelegraphActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Disc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Disc"));
	Disc->SetupAttachment(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderAsset.Succeeded())
	{
		Disc->SetStaticMesh(CylinderAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatAsset(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MatAsset.Succeeded())
	{
		Disc->SetMaterial(0, MatAsset.Object);
	}

	// 只是视觉，不参与任何碰撞
	Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Disc->SetGenerateOverlapEvents(false);
	// 压扁成地面圆盘
	Disc->SetRelativeScale3D(FVector(1.f, 1.f, 0.02f));

	// 外圈环：16 个小方块，先摆在单位圆上，PlayWarning 时按实际半径重摆
	RingSegments.Reserve(RingSegmentCount);
	for (int32 i = 0; i < RingSegmentCount; ++i)
	{
		const FName SegName(*FString::Printf(TEXT("Ring%d"), i));
		UStaticMeshComponent* Seg = CreateDefaultSubobject<UStaticMeshComponent>(SegName);
		Seg->SetupAttachment(RootComponent);
		Seg->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Seg->SetGenerateOverlapEvents(false);
		Seg->SetCastShadow(false);
		if (CylinderAsset.Succeeded())
		{
			Seg->SetStaticMesh(CylinderAsset.Object);
		}
		if (MatAsset.Succeeded())
		{
			Seg->SetMaterial(0, MatAsset.Object);
		}
		// 细长方块：贴地的短柱
		Seg->SetRelativeScale3D(FVector(0.16f, 0.16f, 0.03f));

		RingSegments.Add(Seg);
	}
}

void ATelegraphActor::PlayWarning(float Duration, float Radius, const FLinearColor& Color)
{
	TotalDuration = FMath::Max(0.1f, Duration);
	TargetRadius = FMath::Max(10.f, Radius);
	Elapsed = 0.f;
	bRunning = true;

	DynamicMaterial = Disc->CreateAndSetMaterialInstanceDynamic(0);
	BaseColor = Color;
	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}

	// 外圈：固定半径摆在目标边缘（这才是玩家看到的"圈有多大"）
	// 每段按弧长放大到刚好相接 —— 16 段时代在 4m 半径下会露出 1.5m 的缺口，
	// 看起来像"四个点"而不是"一个固定的圈"，玩家读不出预告区的真实大小。
	const float SegmentSize = (2.f * PI * TargetRadius / RingSegments.Num()) * 1.15f;
	for (int32 i = 0; i < RingSegments.Num(); ++i)
	{
		UStaticMeshComponent* Seg = RingSegments[i];
		if (!Seg)
		{
			continue;
		}
		const float Angle = 2.f * PI * i / RingSegments.Num();
		Seg->SetRelativeLocation(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * TargetRadius);
		Seg->SetWorldScale3D(FVector(SegmentSize / 100.f, SegmentSize / 100.f, 0.03f));

		if (UMaterialInstanceDynamic* SegMID = Seg->CreateAndSetMaterialInstanceDynamic(0))
		{
			SegMID->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}

	// 填充从 0 开始
	Disc->SetWorldScale3D(FVector(0.01f, 0.01f, 0.02f));

	SetActorTickEnabled(true);
}

void ATelegraphActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bRunning)
	{
		return;
	}

	Elapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(Elapsed / TotalDuration, 0.f, 1.f);

	// 10-28 20:21：外圈固定，内部填充从 0 涨到满 —— 读作"倒计时填满即落地"
	const float Radius = FMath::Max(TargetRadius * Alpha, 1.f);
	const float ScaleXY = Radius / BaseCylinderRadius;
	Disc->SetWorldScale3D(FVector(ScaleXY, ScaleXY, 0.02f));

	// 4Hz 闪烁（04 §4：陨石预警 1.5s 内闪 6 次）——几何体风格里这是最低成本的"注意力"手段
	if (DynamicMaterial)
	{
		const float Blink = (FMath::Fmod(Elapsed * 4.f, 1.f) < 0.5f) ? 1.f : 0.45f;
		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), BaseColor * Blink);
	}

	if (Elapsed >= TotalDuration)
	{
		bRunning = false;
		SetActorTickEnabled(false);
		Destroy();
	}
}
