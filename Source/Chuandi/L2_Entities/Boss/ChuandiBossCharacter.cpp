#include "L2_Entities/Boss/ChuandiBossCharacter.h"

#include "L0_Core/ChuandiLog.h"
#include "L0_Core/ChuandiNativeTags.h"
#include "L1_Framework/ChuandiGameMode.h"
#include "L1_Framework/ChuandiTuningData.h"
#include "L2_Entities/World/CoverActor.h"
#include "L3_Systems/BossPerceptionComponent.h"
#include "L3_Systems/BreakMeterComponent.h"
#include "L3_Systems/BossStateMachineComponent.h"
#include "L3_Systems/GAS/Abilities/GA_BossBurrow.h"
#include "L3_Systems/GAS/Abilities/GA_BossCharge.h"
#include "L3_Systems/GAS/Abilities/GA_BossFlame.h"
#include "L3_Systems/GAS/Abilities/GA_BossInstaKill.h"
#include "L3_Systems/GAS/Abilities/GA_BossMelee.h"
#include "L3_Systems/GAS/Abilities/GA_BossMeteor.h"
#include "L3_Systems/GAS/Abilities/GA_BossPull.h"
#include "L3_Systems/GAS/Abilities/GA_BossWave.h"
#include "L3_Systems/GAS/ChuandiAbilitySystemComponent.h"
#include "L3_Systems/GAS/ChuandiCombatLibrary.h"
#include "L3_Systems/GAS/ChuandiAttributeSet.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AChuandiBossCharacter::AChuandiBossCharacter()
{
	// 破防高亮要每帧脉冲，所以 Tick 常开（移动仍然只由 bHasChaseTarget 驱动）
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);   // 10-27 用户要求：先做成与玩家等大

	// ── 机器人模型（与玩家同一套，从 ShooterAI 迁移）──
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BossRobotQuinn(
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BossRobotManny(
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	USkeletalMesh* BossRobotAsset = BossRobotQuinn.Succeeded() ? BossRobotQuinn.Object
		: (BossRobotManny.Succeeded() ? BossRobotManny.Object : nullptr);

	// 11-01 用户要求：Boss 也用模板人形网格（Mannequin / SKM_Quinn）。
	// 几何体作为资产缺失时的兜底；两套都挂上会重叠，所以几何体整段包在 if(!bUseSkeletalBoss) 里。
	static const bool bUseSkeletalBoss = true;

	if (bUseSkeletalBoss && BossRobotAsset)
	{
		USkeletalMeshComponent* Robot = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("RobotMesh"));
		Robot->SetupAttachment(GetCapsuleComponent());
		Robot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Robot->SetRelativeLocation(FVector(0.f, 0.f, -96.f));
		Robot->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
		Robot->SetSkeletalMesh(BossRobotAsset);
		BossRobotMesh = Robot;
		UE_LOG(LogChuandi, Log, TEXT("[Boss] 使用机器人模型 %s"), *BossRobotAsset->GetName());
	}

	// 动画（与玩家同一套 Unarmed 序列）
	{
		static ConstructorHelpers::FObjectFinder<UAnimSequence> BIdle(
			TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
		static ConstructorHelpers::FObjectFinder<UAnimSequence> BJog(
			TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jog/MF_Unarmed_Jog_Fwd.MF_Unarmed_Jog_Fwd"));
		static ConstructorHelpers::FObjectFinder<UAnimSequence> BAtk(
			TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_02.MM_Attack_02"));
		if (BIdle.Succeeded()) { AnimIdle = BIdle.Object; }
		if (BJog.Succeeded())  { AnimJog = BJog.Object; }
		if (BAtk.Succeeded())  { AnimAttack = BAtk.Object; }
	}

	// ── 视觉：人形（圆润版，与玩家的"方块机器人"区分）──
	// 引擎没有人类网格，用 Cylinder 躯干/四肢 + Sphere 头 拼一个人形轮廓。
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatAsset(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	// 朱砂 #C1272D（暖红 = 危险）
	const FLinearColor BossColor(0.76f, 0.15f, 0.18f, 1.f);
	const FLinearColor BossHeadColor(0.88f, 0.26f, 0.28f, 1.f);
	const FLinearColor JointColorBoss(1.f, 0.83f, 0.28f, 1.f);   // 面罩/关节 = 金

	auto MatAssetPtr = MatAsset.Succeeded() ? MatAsset.Object : nullptr;
	// 网格按部件名自动选：躯干/四肢 → Cylinder，头/肩/胸灯 → Sphere，面罩 → Cube
	// 11-01 用户要求：整体缩小 30%（×0.7），上半身再缩 20%（×0.8 → ×0.56）。
	// 以脚底 (0,0,-96) 为锚点做均匀缩放，保证脚不悬空、各部件仍相连。
	auto MakePart = [this, MatAssetPtr](const TCHAR* Name,
		const FVector& Loc, const FVector& Scale, const FLinearColor& Color, bool bUpper = false) -> UStaticMeshComponent*
	{
		const FString PartName(Name);
		const bool bCylinder = PartName.Contains(TEXT("Chest")) || PartName.Contains(TEXT("Waist"))
			|| PartName.Contains(TEXT("Arm")) || PartName.Contains(TEXT("Leg"));
		const bool bCube = PartName.Contains(TEXT("Visor"));
		UStaticMesh* PartMesh = bCube
			? (CubeAsset.Succeeded() ? CubeAsset.Object : nullptr)
			: (bCylinder
				? (CylinderAsset.Succeeded() ? CylinderAsset.Object : nullptr)
				: (SphereAsset.Succeeded() ? SphereAsset.Object : nullptr));

		const FVector Anchor(0.f, 0.f, -96.f);
		const float Factor = bUpper ? (0.7f * 0.8f) : 0.7f;
		const FVector NewScale = Scale * Factor;
		const FVector NewLoc = Anchor + (Loc - Anchor) * Factor;

		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(GetCapsuleComponent());
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetRelativeLocation(NewLoc);
		Part->SetRelativeScale3D(NewScale);
		if (PartMesh)
		{
			Part->SetStaticMesh(PartMesh);
		}
		if (MatAssetPtr)
		{
			// ⚠️ 这里**不能**建 MID：构造函数里的 MID 会被序列化进关卡 Actor，
			// 保存时报 "Illegal reference to private object: MaterialInstanceEditorOnlyData"。
			// 颜色记下来，BeginPlay 再生成 MID（见 ApplyBaseColors）。
			Part->SetMaterial(0, MatAssetPtr);
		}
		GeoPartColors.Add(Color);
		return Part;
	};

	// ── 几何体 Boss（仅当不用模板人形网格时作为兜底）──
	// 人形轮廓：圆润的圆柱躯干 + 球头 + 金面罩，比玩家粗一圈（1.15 倍），颜色朱砂
	if (!bUseSkeletalBoss)
	{
	GeoParts.Add(MakePart(TEXT("Chest"), FVector(0.f, 0.f, 6.f), FVector(0.58f, 0.42f, 0.95f), BossColor, true));
	GeoParts.Add(MakePart(TEXT("Waist"), FVector(0.f, 0.f, -42.f), FVector(0.46f, 0.34f, 0.26f), BossColor));
	GeoParts.Add(MakePart(TEXT("Head"), FVector(0.f, 0.f, 72.f), FVector(0.40f, 0.40f, 0.40f), BossHeadColor, true));
	GeoParts.Add(MakePart(TEXT("Visor"), FVector(22.f, 0.f, 76.f), FVector(0.28f, 0.06f, 0.10f), JointColorBoss, true));
	GeoParts.Add(MakePart(TEXT("ArmL"), FVector(0.f, -46.f, 4.f), FVector(0.16f, 0.16f, 0.78f), BossColor, true));
	GeoParts.Add(MakePart(TEXT("ArmR"), FVector(0.f, 46.f, 4.f), FVector(0.16f, 0.16f, 0.78f), BossColor, true));
	GeoParts.Add(MakePart(TEXT("LegL"), FVector(0.f, -19.f, -70.f), FVector(0.20f, 0.20f, 0.55f), BossColor));
	GeoParts.Add(MakePart(TEXT("LegR"), FVector(0.f, 19.f, -70.f), FVector(0.20f, 0.20f, 0.55f), BossColor));
	}

	AbilitySystemComponent = CreateDefaultSubobject<UChuandiAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(false);

	AttributeSet = CreateDefaultSubobject<UChuandiAttributeSet>(TEXT("AttributeSet"));
	AbilitySystemComponent->AddAttributeSetSubobject(AttributeSet.Get());

	StateMachine = CreateDefaultSubobject<UBossStateMachineComponent>(TEXT("StateMachine"));
	Perception = CreateDefaultSubobject<UBossPerceptionComponent>(TEXT("Perception"));
	BreakMeter = CreateDefaultSubobject<UBreakMeterComponent>(TEXT("BreakMeter"));
}
UAbilitySystemComponent* AChuandiBossCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AChuandiBossCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 几何体部件在这里才生成 MID（构造函数里建会导致关卡保存失败）
	ApplyBaseColors();

	if (!AbilitySystemComponent || !AttributeSet)
	{
		UE_LOG(LogChuandi, Error, TEXT("[Boss] ASC 或 AttributeSet 没创建出来"));
		return;
	}

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	// 破防高亮：给机器人材质建动态实例，Tick 里改 Color（04 §2.1）
	if (BossRobotMesh)
	{
		// M_Geo 系材质（19:22 契约）：Color / Emissive / EmissiveStrength / Roughness
		// ⚠️ 这里是 BeginPlay，不能用 ConstructorHelpers（只能在构造函数里用，否则断言崩溃）
		if (UMaterialInterface* BossMI = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/Chuandi/Materials/MI_Boss.MI_Boss")))
		{
			BossRobotMesh->SetMaterial(0, BossMI);
		}
		BodyMaterial = BossRobotMesh->CreateAndSetMaterialInstanceDynamic(0);

		// 遁地半透明材质：需要你在编辑器里建 M_BossTranslucent（Blend Mode = Translucent，
		// 两个参数：Color / Opacity）。没有这个资产也不影响功能，只影响"半透明"这一层表现。
		const TCHAR* TranslucentPath = TEXT("/Game/Chuandi/Materials/M_BossTranslucent.M_BossTranslucent");
		if (UMaterialInterface* Template = Cast<UMaterialInterface>(
			StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TranslucentPath)))
		{
			TranslucentMaterial = UMaterialInstanceDynamic::Create(Template, this);
			UE_LOG(LogChuandi, Log, TEXT("[Boss] 已加载遁地半透明材质"));
		}
		else
		{
			UE_LOG(LogChuandi, Warning,
				TEXT("[Boss] 没找到 %s —— 遁地时降级为隐藏网格（需在编辑器建这个材质）"), TranslucentPath);
		}
	}

	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float MaxHP = Tuning ? Tuning->BossMaxHealth : 500.f;
	AttributeSet->SetMaxHealth(MaxHP);
	AttributeSet->SetHealth(MaxHP);

	AbilitySystemComponent->GiveAbility(
		FGameplayAbilitySpec(UGA_BossWave::StaticClass(), 1, INDEX_NONE, this));
	AbilitySystemComponent->GiveAbility(
		FGameplayAbilitySpec(UGA_BossMeteor::StaticClass(), 1, INDEX_NONE, this));
	AbilitySystemComponent->GiveAbility(
		FGameplayAbilitySpec(UGA_BossMelee::StaticClass(), 1, INDEX_NONE, this));
	AbilitySystemComponent->GiveAbility(
		FGameplayAbilitySpec(UGA_BossBurrow::StaticClass(), 1, INDEX_NONE, this));
	AbilitySystemComponent->GiveAbility(
		FGameplayAbilitySpec(UGA_BossPull::StaticClass(), 1, INDEX_NONE, this));
	AbilitySystemComponent->GiveAbility(
		FGameplayAbilitySpec(UGA_BossFlame::StaticClass(), 1, INDEX_NONE, this));
	AbilitySystemComponent->GiveAbility(
		FGameplayAbilitySpec(UGA_BossInstaKill::StaticClass(), 1, INDEX_NONE, this));
	AbilitySystemComponent->GiveAbility(
		FGameplayAbilitySpec(UGA_BossCharge::StaticClass(), 1, INDEX_NONE, this));

	AttributeSet->OnDeath.AddUObject(this, &AChuandiBossCharacter::HandleDeath);

	// D6：打开移动，速度从 TuningData 来；灰盒阶段不走 NavMesh，直接朝目标点走
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = Tuning ? Tuning->BossChaseSpeed : 320.f;
		Movement->bRunPhysicsWithNoController = true;
		Movement->SetMovementMode(MOVE_Walking);
	}

	if (AChuandiGameMode* GM = Cast<AChuandiGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->RegisterTarget(this);
	}

	UE_LOG(LogChuandi, Log, TEXT("[Boss] 就绪 %s  HP=%.1f"), *GetNameSafe(this), MaxHP);
}

void AChuandiBossCharacter::FaceActor(const AActor* Target)
{
	if (!Target)
	{
		return;
	}

	const FVector Direction = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		return;
	}

	SetActorRotation(FRotator(0.f, Direction.Rotation().Yaw, 0.f));
}

void AChuandiBossCharacter::FlashColor(const FLinearColor& InColor, float Duration)
{
	FlashColorValue = InColor;
	FlashDuration = FMath::Max(0.01f, Duration);
	FlashTimer = FlashDuration;
}

bool AChuandiBossCharacter::IsStaggered() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < StaggerUntil;
}

void AChuandiBossCharacter::TryBurrow()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	SetCurrentSkill(TEXT("遁地"), 4.f);

	if (!AbilitySystemComponent->TryActivateAbilityByClass(UGA_BossBurrow::StaticClass()))
	{
		UE_LOG(LogChuandi, Warning, TEXT("[Boss] 遁地激活失败"));
	}
}

void AChuandiBossCharacter::TryPull()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (StateMachine && StateMachine->GetState() == EBossState::Dead)
	{
		return;
	}

	FaceActor(UGameplayStatics::GetPlayerPawn(this, 0));

	SetCurrentSkill(TEXT("牵引"), 5.f);

	if (!AbilitySystemComponent->TryActivateAbilityByClass(UGA_BossPull::StaticClass()))
	{
		UE_LOG(LogChuandi, Warning, TEXT("[Boss] 牵引激活失败"));
	}
}

void AChuandiBossCharacter::TryFlame()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (StateMachine && StateMachine->GetState() == EBossState::Dead)
	{
		return;
	}

	FaceActor(UGameplayStatics::GetPlayerPawn(this, 0));

	SetCurrentSkill(TEXT("火焰"), 3.f);

	if (!AbilitySystemComponent->TryActivateAbilityByClass(UGA_BossFlame::StaticClass()))
	{
		UE_LOG(LogChuandi, Warning, TEXT("[Boss] 火焰激活失败"));
	}
}

void AChuandiBossCharacter::TryMelee()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	// 先转身：否则会出现"背对玩家放技能"，感知也会因为角度不过而误判丢视野
	FaceActor(UGameplayStatics::GetPlayerPawn(this, 0));

	SetCurrentSkill(TEXT("横扫普攻"), 2.f);

	if (!AbilitySystemComponent->TryActivateAbilityByClass(UGA_BossMelee::StaticClass()))
	{
		UE_LOG(LogChuandi, Warning, TEXT("[Boss] 近战激活失败"));
	}
}

void AChuandiBossCharacter::TryCastNextSkill()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	const TSubclassOf<UGameplayAbility> Skill = (SkillIndex++ % 2 == 0)
		? TSubclassOf<UGameplayAbility>(UGA_BossWave::StaticClass())
		: TSubclassOf<UGameplayAbility>(UGA_BossMeteor::StaticClass());

	SetCurrentSkill(Skill == UGA_BossWave::StaticClass() ? TEXT("声波") : TEXT("陨石"), 3.5f);

	FaceActor(UGameplayStatics::GetPlayerPawn(this, 0));

	if (!AbilitySystemComponent->TryActivateAbilityByClass(Skill))
	{
		UE_LOG(LogChuandi, Warning, TEXT("[Boss] 技能激活失败: %s"), *GetNameSafe(Skill.Get()));
	}
}

void AChuandiBossCharacter::TryCastWave()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	// 死了就不许再放（状态机会先停，这里是第二道闸）
	if (StateMachine && StateMachine->GetState() == EBossState::Dead)
	{
		return;
	}

	SetCurrentSkill(TEXT("声波"), 3.f);

	const bool bActivated = AbilitySystemComponent->TryActivateAbilityByClass(UGA_BossWave::StaticClass());
	if (!bActivated)
	{
		UE_LOG(LogChuandi, Warning, TEXT("[Boss] 声波激活失败"));
	}
}

void AChuandiBossCharacter::TryCastMeteor()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (StateMachine && StateMachine->GetState() == EBossState::Dead)
	{
		return;
	}

	// 陨石也先转身：圈的位置以玩家为参照，背对着放会让"跑进圈"读不出来
	FaceActor(UGameplayStatics::GetPlayerPawn(this, 0));

	SetCurrentSkill(TEXT("陨石"), 3.5f);

	const bool bActivated = AbilitySystemComponent->TryActivateAbilityByClass(UGA_BossMeteor::StaticClass());
	if (!bActivated)
	{
		UE_LOG(LogChuandi, Warning, TEXT("[Boss] 陨石激活失败"));
	}
}

void AChuandiBossCharacter::ApplyBaseColors()
{
	// 懒创建：只有真在游戏里跑了才生成 MID
	if (GeoMaterials.Num() != GeoParts.Num())
	{
		GeoMaterials.Reset();
		for (int32 i = 0; i < GeoParts.Num(); ++i)
		{
			UStaticMeshComponent* Part = GeoParts[i];
			UMaterialInstanceDynamic* MID = Part ? Part->CreateAndSetMaterialInstanceDynamic(0) : nullptr;
			if (MID && GeoPartColors.IsValidIndex(i))
			{
				MID->SetVectorParameterValue(TEXT("Color"), GeoPartColors[i]);
			}
			GeoMaterials.Add(MID);
		}
	}
}

void AChuandiBossCharacter::ApplyBodyColor(const FLinearColor& Color, float EmissiveStrength)
{
	// 先确保 MID 已经建好（BeginPlay 会调一次 ApplyBaseColors）
	if (GeoMaterials.Num() != GeoParts.Num())
	{
		ApplyBaseColors();
	}

	for (UMaterialInstanceDynamic* MID : GeoMaterials)
	{
		if (!MID)
		{
			continue;
		}
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		MID->SetScalarParameterValue(TEXT("EmissiveStrength"), EmissiveStrength);
	}

	// 万一以后切回骨架网格，也一起上色
	if (BodyMaterial)
	{
		BodyMaterial->SetVectorParameterValue(TEXT("Color"), Color);
		BodyMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), EmissiveStrength);
	}
}

void AChuandiBossCharacter::SetBodyVisible(bool bVisible)
{
	for (UStaticMeshComponent* Part : GeoParts)
	{
		if (Part)
		{
			Part->SetVisibility(bVisible);
		}
	}

	if (BossRobotMesh)
	{
		BossRobotMesh->SetVisibility(bVisible);
	}
}

void AChuandiBossCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bHasChaseTarget)
	{
		MoveTowards(ChaseTarget, DeltaSeconds);
	}

	UpdateRobotAnim(DeltaSeconds);

	// 遁地表现（04 §2.1：不可选中）。几何体没有半透明材质 → 直接隐藏，
	// 位置靠地面预警圈提示（07 §2.7 允许的降级方案）。
	if (AbilitySystemComponent)
	{
		const bool bBurrowed = AbilitySystemComponent->HasMatchingGameplayTag(
			ChuandiTags::State_Burrowed.GetTag());

		if (bBurrowed && !bBurrowVisualActive)
		{
			bBurrowVisualActive = true;
			SetBodyVisible(false);
		}
		else if (!bBurrowed && bBurrowVisualActive)
		{
			bBurrowVisualActive = false;
			SetBodyVisible(true);
			ApplyBodyColor(FLinearColor(0.76f, 0.15f, 0.18f, 1.f), 0.f);
		}
	}

	// 破防高亮（04 §2.1 破防 = #FFD447 亮金；§4 自发光脉冲 2Hz）
	if (AbilitySystemComponent)
	{
		const bool bBroken = AbilitySystemComponent->HasMatchingGameplayTag(
			ChuandiTags::State_Broken.GetTag());

		// 即时闪色优先：受击闪白 0.08s / 弹反成功闪金 0.2s（08 §3）
		if (FlashTimer > 0.f)
		{
			FlashTimer -= DeltaSeconds;
			const float Alpha = FMath::Clamp(FlashTimer / FMath::Max(FlashDuration, 0.01f), 0.f, 1.f);
			const FLinearColor Lerped = FMath::Lerp(FLinearColor(0.76f, 0.15f, 0.18f, 1.f), FlashColorValue, Alpha);
			ApplyBodyColor(Lerped, 4.f * Alpha);
		}
		else if (bBroken)
		{
			const float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
			const float Pulse = 0.65f + 0.35f * FMath::Abs(FMath::Sin(Time * PI * 4.f));   // 2Hz
			// 破防：亮金 + 自发光脉冲（11 表现层「全场最亮的就是它」）
			ApplyBodyColor(FLinearColor(1.f, 0.83f, 0.28f, 1.f), 3.f * Pulse);
		}
		else
		{
			// 常态：恢复每个部件自己的基准色（朱砂 / 亮红头 / 金面罩）
			ApplyBaseColors();
			for (UMaterialInstanceDynamic* MID : GeoMaterials)
			{
				if (MID)
				{
					MID->SetScalarParameterValue(TEXT("EmissiveStrength"), 0.f);
				}
			}
		}
	}
}

void AChuandiBossCharacter::SetChaseTarget(const FVector& TargetLocation)
{
	ChaseTarget = TargetLocation;

	if (!bHasChaseTarget)
	{
		bHasChaseTarget = true;
		UE_LOG(LogChuandi, Log, TEXT("[BossMove] 收到追击目标，Tick=%d"),
			IsActorTickEnabled() ? 1 : 0);
	}
}

void AChuandiBossCharacter::ClearChaseTarget()
{
	// 只清目标，不关 Tick —— Tick 还要负责破防高亮脉冲
	bHasChaseTarget = false;
}

void AChuandiBossCharacter::PlayActionAnim(UAnimSequence* Seq, float Duration)
{
	if (!BossRobotMesh || !Seq)
	{
		return;
	}
	ActionAnim = Seq;
	ActionAnimTimer = Duration > 0.f ? Duration : Seq->GetPlayLength();
	BossRobotMesh->PlayAnimation(Seq, /*bLooping=*/false);
	CurrentAnim = Seq;
}

void AChuandiBossCharacter::UpdateRobotAnim(float DeltaSeconds)
{
	if (!BossRobotMesh)
	{
		return;
	}

	ActionAnimTimer = FMath::Max(0.f, ActionAnimTimer - DeltaSeconds);

	UAnimSequence* Want = nullptr;
	bool bLoop = true;
	if (ActionAnimTimer > 0.f)
	{
		Want = ActionAnim;
		bLoop = false;
	}
	else if (GetVelocity().Size2D() > 50.f)
	{
		Want = AnimJog;
	}
	else
	{
		Want = AnimIdle;
	}

	if (Want && Want != CurrentAnim)
	{
		CurrentAnim = Want;
		BossRobotMesh->PlayAnimation(Want, bLoop);
	}
}

void AChuandiBossCharacter::MoveTowards(const FVector& TargetLocation, float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();

	// 撞碎石后的停顿：原地不动（07 §3.3-B）
	if (Now < StaggerUntil)
	{
		return;
	}

	const FVector Direction = (TargetLocation - GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		return;
	}

	const float Speed = GetCharacterMovement() ? GetCharacterMovement()->MaxWalkSpeed : 320.f;

	// ★ 直接平移，不走 CharacterMovement：
	//   灰盒阶段没必要为"直线追击"承担 Controller / 无控制器物理 / 重力 这些前置条件，
	//   直接 SetActorLocation 行为确定，也方便以后换成 NavMesh 时只替换这一个函数。
	FHitResult Hit;
	const FVector OldLocation = GetActorLocation();
	// 直线冲锋模式（07 §3.3-B 卡住兜底）：无视碰撞直接推进 1.5s
	const bool bCharging = Now < ChargeUntil;
	const bool bMoved = SetActorLocation(OldLocation + Direction * Speed * DeltaSeconds,
		/*bSweep=*/!bCharging, &Hit);

	// 前方有碎石 → 直接撞碎 + 停顿 0.5s（不绕行，07 §3.3-B）
	if (!bMoved && !bCharging)
	{
		if (AChuandiCoverActor* Cover = Cast<AChuandiCoverActor>(Hit.GetActor()))
		{
			const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
			const float Stagger = Tuning ? Tuning->BossCoverBreakStagger : 0.5f;

			Cover->BreakCover();
			StaggerUntil = Now + Stagger;

			// 破防条不增加（07 §3.3-B：撞碎石不算传递）
			UE_LOG(LogChuandi, Log, TEXT("[Boss] 撞碎碎石 → 停顿 %.1fs（破防条不增加）"), Stagger);
			return;
		}
	}

	// 兜底：扫掠没被任何 Actor 挡住却失败（常见于胶囊起始位置轻微穿地），改用瞬移，避免完全卡死。
	// 被 Actor 挡住时不兜底——那多半就是玩家，挡住是对的。
	if (!bMoved && !Hit.GetActor() && !bCharging)
	{
		SetActorLocation(OldLocation + Direction * Speed * DeltaSeconds, /*bSweep=*/false);
	}

	// ── 卡住检测（07 §3.3-B）：每 0.5s 采样，连续 2s 位移 < 0.5m → 直线冲锋 1.5s
	StallSampleTimer += DeltaSeconds;
	if (StallSampleTimer >= 0.5f)
	{
		const float Moved = FVector::Dist2D(GetActorLocation(), StallSamplePos);
		if (Moved < 50.f)
		{
			StallAccum += StallSampleTimer;
		}
		else
		{
			StallAccum = 0.f;
		}

		StallSamplePos = GetActorLocation();
		StallSampleTimer = 0.f;

		if (StallAccum >= 2.f && !bCharging)
		{
			const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
			const float ChargeTime = Tuning ? Tuning->BossStallChargeTime : 1.5f;
			ChargeUntil = Now + ChargeTime;
			StallAccum = 0.f;
			UE_LOG(LogChuandi, Warning, TEXT("[Boss] 卡住 2s（位移 <0.5m）→ 直线冲锋 %.1fs"), ChargeTime);
		}
	}

	// 诊断：每秒一次，说明 Tick 是否在跑、位移是否成功、被谁挡住
	MoveLogAccum += DeltaSeconds;
	if (MoveLogAccum >= 1.f)
	{
		MoveLogAccum = 0.f;
		UE_LOG(LogChuandi, Log,
			TEXT("[BossMove] tick=%d 距离目标=%.0f 位移=%.1f 成功=%d 阻挡=%s"),
			IsActorTickEnabled() ? 1 : 0,
			FVector::Dist2D(GetActorLocation(), TargetLocation),
			FVector::Dist2D(GetActorLocation(), OldLocation),
			bMoved ? 1 : 0,
			Hit.GetActor() ? *GetNameSafe(Hit.GetActor()) : TEXT("无"));
	}

	// 转身：给玩家"它正在拐过来"的读秒
	const FRotator Desired(0.f, Direction.Rotation().Yaw, 0.f);
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), Desired, DeltaSeconds, 6.f));

	if (!bLoggedMovement)
	{
		bLoggedMovement = true;
		UE_LOG(LogChuandi, Log, TEXT("[Boss] 开始移动（速度 %.0f cm/s）"), Speed);
	}
}

void AChuandiBossCharacter::StopMoving()
{
	ClearChaseTarget();
}

void AChuandiBossCharacter::HandleDeath(AActor* DeadActor)
{
	UE_LOG(LogChuandi, Log, TEXT("[Boss] %s 被击杀"), *GetNameSafe(DeadActor));

	// 1) 打断正在施法的技能（预警圈会随任务一起销毁）
	AbilitySystemComponent->CancelAllAbilities();

	// 2) 打死亡 tag：伤害入口、状态机、以后的表现层都靠它判断
	AbilitySystemComponent->AddLooseGameplayTag(ChuandiTags::State_Dead.GetTag());

	// 3) 状态机停机（IsControlledByTag() 里 Dead 直接返回）
	if (StateMachine)
	{
		StateMachine->SetState(EBossState::Dead);
	}

	GetCharacterMovement()->DisableMovement();
	if (BossRobotMesh) { BossRobotMesh->SetVisibility(false); }

	if (AChuandiGameMode* GM = Cast<AChuandiGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->NotifyTargetDied(this);
	}
}

// ─────────────────────────────────────────────────────────────
// 10-29 新增：全屏秒杀 + 冲撞破柱
// ─────────────────────────────────────────────────────────────

void AChuandiBossCharacter::TryInstaKill()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (StateMachine && StateMachine->GetState() == EBossState::Dead)
	{
		return;
	}

	SetCurrentSkill(TEXT("全屏秒杀"), 11.f);

	if (!AbilitySystemComponent->TryActivateAbilityByClass(UGA_BossInstaKill::StaticClass()))
	{
		UE_LOG(LogChuandi, Warning, TEXT("[Boss] 全屏秒杀激活失败"));
	}
}

void AChuandiBossCharacter::TryCharge()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (StateMachine && StateMachine->GetState() == EBossState::Dead)
	{
		return;
	}

	SetCurrentSkill(TEXT("冲撞"), 5.5f);

	if (!AbilitySystemComponent->TryActivateAbilityByClass(UGA_BossCharge::StaticClass()))
	{
		UE_LOG(LogChuandi, Warning, TEXT("[Boss] 冲撞激活失败"));
	}
}

void AChuandiBossCharacter::SetCurrentSkill(const FString& SkillName, float Duration)
{
	CurrentSkillName = SkillName;
	if (const UWorld* World = GetWorld())
	{
		CurrentSkillExpire = World->GetTimeSeconds() + FMath::Max(0.1f, Duration);
	}
	else
	{
		CurrentSkillExpire = 0.0;
	}
}

FString AChuandiBossCharacter::GetCurrentSkillName() const
{
	const UWorld* World = GetWorld();
	if (World && CurrentSkillExpire > 0.0 && World->GetTimeSeconds() < CurrentSkillExpire)
	{
		return CurrentSkillName;
	}
	return FString();
}

void AChuandiBossCharacter::StartInstaKillWarning(float Duration)
{
	if (const UWorld* World = GetWorld())
	{
		InstaKillWarningEnd = World->GetTimeSeconds() + FMath::Max(0.1f, Duration);
	}
}

void AChuandiBossCharacter::ClearInstaKillWarning()
{
	InstaKillWarningEnd = 0.0;
}

bool AChuandiBossCharacter::IsInstaKillWarningActive() const
{
	const UWorld* World = GetWorld();
	return World && InstaKillWarningEnd > 0.0 && World->GetTimeSeconds() < InstaKillWarningEnd;
}

float AChuandiBossCharacter::GetInstaKillWarningRemaining() const
{
	const UWorld* World = GetWorld();
	if (!World || InstaKillWarningEnd <= 0.0)
	{
		return 0.f;
	}
	return FMath::Max(0.f, static_cast<float>(InstaKillWarningEnd - World->GetTimeSeconds()));
}

bool AChuandiBossCharacter::NotifyPillarDestroyedByCharge()
{
	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const int32 Needed = Tuning ? Tuning->ChargeBreakCount : 3;

	++PillarsSmashedByCharge;
	UE_LOG(LogChuandi, Log, TEXT("[Charge] 撞毁柱子 %d / %d"), PillarsSmashedByCharge, Needed);

	if (PillarsSmashedByCharge >= Needed)
	{
		PillarsSmashedByCharge = 0;
		if (BreakMeter)
		{
			// 直接灌满：破防的判定、时长、倍率全部复用现有那套
			BreakMeter->AddGauge(BreakMeter->GetMaxMeter());
		}
		UE_LOG(LogChuandi, Log, TEXT("[Charge] 撞满 %d 根 → 直接破防"), Needed);
		return true;
	}

	return false;
}

void AChuandiBossCharacter::ShowUnlockCue(const FString& SkillName, float Duration)
{
	if (const UWorld* World = GetWorld())
	{
		UnlockCueText = SkillName;
		UnlockCueEnd = World->GetTimeSeconds() + FMath::Max(0.2f, Duration);
	}
}

bool AChuandiBossCharacter::IsUnlockCueActive() const
{
	const UWorld* World = GetWorld();
	return World && UnlockCueEnd > 0.0 && World->GetTimeSeconds() < UnlockCueEnd;
}

float AChuandiBossCharacter::GetUnlockCueRemaining() const
{
	const UWorld* World = GetWorld();
	if (!World || UnlockCueEnd <= 0.0)
	{
		return 0.f;
	}
	return FMath::Max(0.f, static_cast<float>(UnlockCueEnd - World->GetTimeSeconds()));
}

void AChuandiBossCharacter::StartChargeLock(float Duration)
{
	if (const UWorld* World = GetWorld())
	{
		ChargeLockEnd = World->GetTimeSeconds() + FMath::Max(0.1f, Duration);
	}
}

void AChuandiBossCharacter::ClearChargeLock()
{
	ChargeLockEnd = 0.0;
}

bool AChuandiBossCharacter::IsChargeLockActive() const
{
	const UWorld* World = GetWorld();
	return World && ChargeLockEnd > 0.0 && World->GetTimeSeconds() < ChargeLockEnd;
}

float AChuandiBossCharacter::GetChargeLockRemaining() const
{
	const UWorld* World = GetWorld();
	if (!World || ChargeLockEnd <= 0.0)
	{
		return 0.f;
	}
	return FMath::Max(0.f, static_cast<float>(ChargeLockEnd - World->GetTimeSeconds()));
}

void AChuandiBossCharacter::ApplyStagger(float Seconds)
{
	if (const UWorld* World = GetWorld())
	{
		StaggerUntil = World->GetTimeSeconds() + FMath::Max(0.f, Seconds);
	}
}
