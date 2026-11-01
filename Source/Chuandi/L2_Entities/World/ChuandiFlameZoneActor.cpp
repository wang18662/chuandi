#include "L2_Entities/World/ChuandiFlameZoneActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "L3_Systems/GAS/ChuandiCombatLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// 引擎自带方块：100×100×100（缩放前）
	constexpr float BaseCubeSize = 100.f;

	// 板厚 2cm / 边条宽 6cm
	constexpr float SlabThickness = 0.02f;
	constexpr float FrameBarWidth = 6.f;

	// 两个阶段的配色：边框比填充亮一档，读作"边界是确定的、里面正在填充"
	const FLinearColor WarnFrameColor(0.85f, 0.18f, 0.10f, 1.f);
	const FLinearColor WarnFillColor(0.42f, 0.07f, 0.03f, 1.f);
	const FLinearColor BurnColor(1.f, 0.35f, 0.08f, 1.f);
}

AChuandiFlameZoneActor::AChuandiFlameZoneActor()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatAsset(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	auto MakeSlab = [&](const FName& Name) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Slab = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Slab->SetupAttachment(Root);
		if (CubeAsset.Succeeded())
		{
			Slab->SetStaticMesh(CubeAsset.Object);
		}
		if (MatAsset.Succeeded())
		{
			Slab->SetMaterial(0, MatAsset.Object);
		}
		// 纯视觉：不碰撞（否则会变成一堵看不见的墙）、不投影
		Slab->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Slab->SetGenerateOverlapEvents(false);
		Slab->SetCastShadow(false);
		return Slab;
	};

	Fill = MakeSlab(TEXT("Fill"));

	// 4 根边条：0/1 = 两条长边，2/3 = 两个端头
	static const TCHAR* FrameNames[] = { TEXT("FrameL0"), TEXT("FrameL1"), TEXT("FrameE0"), TEXT("FrameE1") };
	for (int32 i = 0; i < 4; ++i)
	{
		Frame.Add(MakeSlab(FName(FrameNames[i])));
	}
}

void AChuandiFlameZoneActor::BeginStrip(float InLength, float InWidth, float InWarningTime, float InBurnTime,
	float InTickInterval, float InTickDamage, AActor* InInstigator)
{
	Length = FMath::Max(100.f, InLength);
	Width = FMath::Max(50.f, InWidth);
	WarningTime = FMath::Max(0.01f, InWarningTime);
	BurnTime = FMath::Max(0.1f, InBurnTime);
	TickInterval = FMath::Max(0.05f, InTickInterval);
	TickDamage = InTickDamage;
	DamageInstigator = InInstigator;

	Elapsed = 0.f;
	TickAccum = 0.f;
	bBurning = false;

	// ── 边框：尺寸一次定死，预警全程不变（这就是"最终伤害区有多大"）──
	const float HalfL = Length * 0.5f;
	const float HalfW = Width * 0.5f;

	Frame[0]->SetRelativeLocation(FVector(0.f, HalfW, 0.f));
	Frame[0]->SetRelativeScale3D(FVector(Length / BaseCubeSize, FrameBarWidth / BaseCubeSize, 0.03f));

	Frame[1]->SetRelativeLocation(FVector(0.f, -HalfW, 0.f));
	Frame[1]->SetRelativeScale3D(FVector(Length / BaseCubeSize, FrameBarWidth / BaseCubeSize, 0.03f));

	Frame[2]->SetRelativeLocation(FVector(HalfL, 0.f, 0.f));
	Frame[2]->SetRelativeScale3D(FVector(FrameBarWidth / BaseCubeSize, Width / BaseCubeSize, 0.03f));

	Frame[3]->SetRelativeLocation(FVector(-HalfL, 0.f, 0.f));
	Frame[3]->SetRelativeScale3D(FVector(FrameBarWidth / BaseCubeSize, Width / BaseCubeSize, 0.03f));

	// ── 材质实例：填充 1 个 + 边框 4 个（分开存，填充不跟着边框闪）──
	FillMaterial = Fill->CreateAndSetMaterialInstanceDynamic(0);
	if (FillMaterial)
	{
		FillMaterial->SetVectorParameterValue(TEXT("Color"), WarnFillColor);
	}

	FrameMaterials.Reset();
	for (int32 i = 0; i < Frame.Num(); ++i)
	{
		if (UMaterialInstanceDynamic* SegMID = Frame[i]->CreateAndSetMaterialInstanceDynamic(0))
		{
			SegMID->SetVectorParameterValue(TEXT("Color"), WarnFrameColor);
			FrameMaterials.Add(SegMID);
		}
	}

	// 填充从 0 开始长
	UpdateFill(0.f);
}

void AChuandiFlameZoneActor::UpdateFill(float Alpha)
{
	const float Clamped = FMath::Clamp(Alpha, 0.f, 1.f);
	const float CurrentLength = FMath::Max(1.f, Length * Clamped);

	Fill->SetRelativeScale3D(FVector(CurrentLength / BaseCubeSize, Width / BaseCubeSize, SlabThickness));

	// 贴着 -X 端往 +X 长（起点在靠 Boss 那一侧），中心随长度前移
	Fill->SetRelativeLocation(FVector(-Length * 0.5f + CurrentLength * 0.5f, 0.f, 0.f));
}

void AChuandiFlameZoneActor::SetFrameColor(const FLinearColor& Color)
{
	for (UMaterialInstanceDynamic* MID : FrameMaterials)
	{
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}
}

void AChuandiFlameZoneActor::SetFillColor(const FLinearColor& Color)
{
	if (FillMaterial)
	{
		FillMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

bool AChuandiFlameZoneActor::IsPlayerInside() const
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return false;
	}

	// 局部坐标判定：Actor 已经按 Boss 朝向旋转，这里只看本地 X/Y 是否在半个长度/宽度内。
	// Z 也要判：柱子有 3m 高，站在柱子上不该被地上的火烧到（只留 ±1.5m 容差给台阶/坡）。
	// 用 NoScale 版本：返回世界单位，直接和 Length/Width 比，不受 Z 压扁影响。
	const FVector Local = GetActorTransform().InverseTransformPositionNoScale(Player->GetActorLocation());
	return FMath::Abs(Local.X) <= Length * 0.5f
		&& FMath::Abs(Local.Y) <= Width * 0.5f
		&& FMath::Abs(Local.Z) <= 150.f;
}

void AChuandiFlameZoneActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Elapsed += DeltaSeconds;

	if (!bBurning)
	{
		// 预警：填充随进度长满（这就是倒计时），边框 4Hz 闪提示"还没落地"
		const float Alpha = FMath::Clamp(Elapsed / WarningTime, 0.f, 1.f);
		UpdateFill(Alpha);

		const float Blink = (FMath::Fmod(Elapsed * 4.f, 1.f) < 0.5f) ? 1.f : 0.45f;
		SetFrameColor(WarnFrameColor * Blink);
		SetFillColor(WarnFillColor);

		if (Elapsed >= WarningTime)
		{
			bBurning = true;
			Elapsed = 0.f;
			TickAccum = 0.f;
			UpdateFill(1.f);
			SetFrameColor(BurnColor);
			SetFillColor(BurnColor);
		}
		return;
	}

	// 燃烧：2Hz 呼吸 + 最后 0.4s 淡出（避免"啪一下消失"读成 bug）
	const float Pulse = 0.7f + 0.3f * FMath::Abs(FMath::Sin(Elapsed * PI * 2.f));
	const float Fade = FMath::Clamp((BurnTime - Elapsed) / 0.4f, 0.f, 1.f);
	SetFrameColor(BurnColor * Pulse * Fade);
	SetFillColor(BurnColor * Pulse * Fade);

	// 伤害按固定节拍结算：站满 = BurnTime / TickInterval × TickDamage
	TickAccum += DeltaSeconds;
	while (TickAccum >= TickInterval)
	{
		TickAccum -= TickInterval;
		APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
		if (Player && IsPlayerInside())
		{
			UChuandiCombatLibrary::ApplyDamage(DamageInstigator.Get(), Player, TickDamage);
		}
	}

	if (Elapsed >= BurnTime)
	{
		Destroy();
	}
}
