#include "L2_Entities/World/CoverActor.h"

#include "L0_Core/ChuandiLog.h"
#include "L1_Framework/ChuandiTuningData.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AChuandiCoverActor::AChuandiCoverActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Rock = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rock"));
	RootComponent = Rock;
	Rock->SetRelativeScale3D(FVector(1.4f, 1.4f, 2.2f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeAsset.Succeeded())
	{
		Rock->SetStaticMesh(CubeAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatAsset(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MatAsset.Succeeded())
	{
		Rock->SetMaterial(0, MatAsset.Object);

		// 优先用 M_Geo 系材质实例（19:22 契约），找不到才退回 BasicShape 调色
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> RockMI(
			TEXT("/Game/Chuandi/Materials/MI_Rock.MI_Rock"));
		if (RockMI.Succeeded())
		{
			Rock->SetMaterial(0, RockMI.Object);
		}

		// 04 §2.1：碎石 = 石灰 #5A5A52（中性，明确"这不是敌人"）
		if (UMaterialInstanceDynamic* MID = Rock->CreateAndSetMaterialInstanceDynamic(0))
		{
			// 交接 16:46：碎石归"淡"层 #3F3F39
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.247f, 0.247f, 0.224f, 1.f));
		}
	}

	// 挡人、挡声波、挡视线
	Rock->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Rock->SetCollisionResponseToAllChannels(ECR_Block);
	Rock->SetGenerateOverlapEvents(false);

	// 04 §1：碎石 = 3–5 个不规则 Cube 组合（单块 Cube 看起来像"箱子"，不像岩石）
	// 只有主块带碰撞，其余是纯装饰，避免挡路范围比视觉更大
	const FVector Chunks[4] =
	{
		FVector(0.35f, -0.45f, 0.30f),   // 右上
		FVector(-0.50f, 0.30f, -0.10f),  // 左下
		FVector(0.15f, 0.55f, 0.75f),    // 顶上
		FVector(-0.30f, -0.55f, 0.90f)   // 顶部另一块
	};

	for (int32 i = 0; i < 4; ++i)
	{
		const FName ChunkName(*FString::Printf(TEXT("Chunk%d"), i));
		UStaticMeshComponent* Chunk = CreateDefaultSubobject<UStaticMeshComponent>(ChunkName);
		Chunk->SetupAttachment(Rock);
		Chunk->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		if (CubeAsset.Succeeded())
		{
			Chunk->SetStaticMesh(CubeAsset.Object);
			Chunk->SetMaterial(0, MatAsset.Object);
		}

		// 每块大小、朝向、位置都不同 → 不规则
		const float SizeX = 0.55f + 0.15f * (i % 3);
		const float SizeY = 0.45f + 0.12f * ((i + 1) % 3);
		Chunk->SetRelativeScale3D(FVector(SizeX, SizeY, 0.6f + 0.1f * i));
		Chunk->SetRelativeLocation(Chunks[i]);
		Chunk->SetRelativeRotation(FRotator(15.f * i, 25.f * i, 10.f * i));
	}
}

void AChuandiCoverActor::BeginPlay()
{
	Super::BeginPlay();

	// 10-29 用户改：碎石**不再随时间自动消失**（原来是 15s），
	// 只有三种情况会消失：被声波打碎 / 被 Boss 冲撞撞碎 / 挡下全屏秒杀后被消耗。
	if (const UWorld* World = GetWorld())
	{
		SpawnTime = World->GetTimeSeconds();
	}
}

FVector AChuandiCoverActor::GetStandTopLocation() const
{
	// Rock 是根组件（Actor 缩放 1），Actor 位置 = 柱体中心 → 顶面 = 中心 + 半高
	return GetActorLocation() + FVector(0.f, 0.f, RubbleHeight * 0.5f);
}

void AChuandiCoverActor::ConsumePillar(const TCHAR* Reason)
{
	if (!bIntact)
	{
		return;
	}

	bIntact = false;
	UE_LOG(LogChuandi, Log, TEXT("[Cover] %s 被消耗（%s）"), *GetNameSafe(this), Reason);
	Destroy();
}

void AChuandiCoverActor::ConfigureRubble(float InHeight, float InFootprint)
{
	RubbleHeight = FMath::Clamp(InHeight, 40.f, 400.f);
	RubbleFootprint = FMath::Clamp(InFootprint, 60.f, 400.f);

	if (!Rock)
	{
		return;
	}

	// Rock 是根组件、Actor 缩放为 1，所以相对缩放 = 世界尺寸。
	// 顶面平整（只有 Yaw 旋转）→ 高度 ≤ 跳跃上限时玩家能站上去，这就是"高度差"。
	Rock->SetRelativeScale3D(FVector(
		RubbleFootprint / 100.f, RubbleFootprint / 100.f, RubbleHeight / 100.f));

	UE_LOG(LogChuandi, Log, TEXT("[Cover] 碎石体量：高 %.0fcm 底边 %.0fcm"), RubbleHeight, RubbleFootprint);
}

void AChuandiCoverActor::BreakCover()
{
	if (!bIntact)
	{
		return;
	}

	bIntact = false;
	UE_LOG(LogChuandi, Log, TEXT("[Cover] %s 被声波击碎"), *GetNameSafe(this));
	Destroy();
}
