#include "L2_Entities/Player/ChuandiPlayerCharacter.h"

#include "L0_Core/ChuandiLog.h"
#include "L0_Core/ChuandiNativeTags.h"
#include "L1_Framework/ChuandiGameMode.h"
#include "L1_Framework/ChuandiTuningData.h"
#include "L2_Entities/Boss/ChuandiBossCharacter.h"
#include "L2_Entities/World/CoverActor.h"
#include "L2_Entities/World/TrapIndicatorActor.h"
#include "L3_Systems/GAS/Abilities/GA_PlayerAttack.h"
#include "L3_Systems/GAS/Abilities/GA_PlayerDodge.h"
#include "L3_Systems/GAS/Abilities/GA_PlayerParry.h"
#include "L3_Systems/GAS/Abilities/GA_PlayerPlaceTrap.h"
#include "L3_Systems/GAS/Abilities/GA_PlayerSkill.h"
#include "L3_Systems/GAS/ChuandiAbilitySystemComponent.h"
#include "L3_Systems/GAS/ChuandiAttributeSet.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "EngineUtils.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const TCHAR* MoveActionPath  = TEXT("/Game/Chuandi/Input/IA_Move.IA_Move");
	const TCHAR* LookActionPath  = TEXT("/Game/Chuandi/Input/IA_Look.IA_Look");
	const TCHAR* JumpActionPath  = TEXT("/Game/Chuandi/Input/IA_Jump.IA_Jump");
	const TCHAR* AttackActionPath = TEXT("/Game/Chuandi/Input/IA_Attack.IA_Attack");
	const TCHAR* ParryActionPath = TEXT("/Game/Chuandi/Input/IA_Parry.IA_Parry");
	const TCHAR* TrapActionPath = TEXT("/Game/Chuandi/Input/IA_Trap.IA_Trap");
	const TCHAR* DodgeActionPath = TEXT("/Game/Chuandi/Input/IA_Dodge.IA_Dodge");
	const TCHAR* SkillActionPath = TEXT("/Game/Chuandi/Input/IA_Skill.IA_Skill");

	UInputAction* LoadInputAction(const TCHAR* Path)
	{
		UInputAction* Action = Cast<UInputAction>(StaticLoadObject(UInputAction::StaticClass(), nullptr, Path));
		if (!Action)
		{
			UE_LOG(LogChuandi, Warning,
				TEXT("[Input] 找不到 %s。请在 Content/Chuandi/Input/ 下按这个路径建 InputAction。"), Path);
		}
		return Action;
	}
}

AChuandiPlayerCharacter::AChuandiPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// ── 机器人模型（从 ShooterAI 迁移过来的 UE5 人形网格）──
	// 资产不存在时自动降级到下面的几何体拼装，不会隐形
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> RobotQuinn(
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> RobotManny(
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	USkeletalMesh* RobotAsset = RobotQuinn.Succeeded() ? RobotQuinn.Object
		: (RobotManny.Succeeded() ? RobotManny.Object : nullptr);

	if (!RobotAsset)
	{
		UE_LOG(LogChuandi, Error,
			TEXT("[Player] 找不到机器人模型（/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple）。")
			TEXT("角色会不可见——先把资产从 ShooterAI 迁移过来。"));
	}

	// 11-01 用户要求：人物用模板人形网格（Mannequin / SKM_Quinn）。
	// 几何体作为资产缺失时的兜底；两套都挂上会重叠，所以几何体整段包在 if(!bUseSkeletalRobot) 里。
	static const bool bUseSkeletalRobot = true;

	if (bUseSkeletalRobot && RobotAsset)
	{
		USkeletalMeshComponent* Robot = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("RobotMesh"));
		Robot->SetupAttachment(GetCapsuleComponent());
		Robot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// 人形网格原点在脚底，胶囊中心在 -96
		Robot->SetRelativeLocation(FVector(0.f, 0.f, -96.f));
		Robot->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
		Robot->SetSkeletalMesh(RobotAsset);
		RobotMesh = Robot;

		// M_Geo 系材质（19:22 契约）：玩家 = MI_Player 天青
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> PlayerMI(
			TEXT("/Game/Chuandi/Materials/MI_Player.MI_Player"));
		if (PlayerMI.Succeeded())
		{
			Robot->SetMaterial(0, PlayerMI.Object);
		}

		UE_LOG(LogChuandi, Log, TEXT("[Player] 使用机器人模型 %s"), *RobotAsset->GetName());
	}

	// 动画序列（从 ShooterAI 复制过来的 UE5 Unarmed 动画）
	{
		static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleSeq(
			TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
		static ConstructorHelpers::FObjectFinder<UAnimSequence> JogSeq(
			TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jog/MF_Unarmed_Jog_Fwd.MF_Unarmed_Jog_Fwd"));
		static ConstructorHelpers::FObjectFinder<UAnimSequence> JumpSeq(
			TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Jump.MM_Jump"));
		static ConstructorHelpers::FObjectFinder<UAnimSequence> DashSeq(
			TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Dash.MM_Dash"));
		static ConstructorHelpers::FObjectFinder<UAnimSequence> Atk1(
			TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01"));
		static ConstructorHelpers::FObjectFinder<UAnimSequence> Atk2(
			TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_02.MM_Attack_02"));
		static ConstructorHelpers::FObjectFinder<UAnimSequence> Atk3(
			TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_03.MM_Attack_03"));

		if (IdleSeq.Succeeded()) { AnimIdle = IdleSeq.Object; }
		if (JogSeq.Succeeded())  { AnimJog = JogSeq.Object; }
		if (JumpSeq.Succeeded()) { AnimJump = JumpSeq.Object; }
		if (DashSeq.Succeeded()) { AnimDash = DashSeq.Object; }
		if (Atk1.Succeeded()) { AnimAttacks.Add(Atk1.Object); }   // 挥拳：仅保留一段攻击动画
		// 11-01 用户要求：攻击动画只做挥拳，故不再加入 Atk2/Atk3（如需换其它出拳，改这一行即可）
	}

	// ── 视觉：机器人造型（方块人形，仅当不用模板人形网格时作为兜底）──
	// 引擎 /Engine/BasicShapes 只有 Cone/Cube/Cylinder/Plane/Sphere，没有 Capsule 也没有人形网格，
	// 所以用 8 个方块拼一个机器人：方头 + 方躯干 + 两臂 + 两腿 + 面罩 + 胸灯。
	if (!bUseSkeletalRobot)
	{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PartMatAsset(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	// 机器人本体色：冷白青（04 交接 16:44 提亮版）
	const FLinearColor RobotColor(0.78f, 0.88f, 0.93f, 1.f);
	const FLinearColor JointColor(1.f, 0.83f, 0.28f, 1.f);   // 关节/面罩 = 金

	// 11-01 用户要求：整体缩小 30%（×0.7），上半身再缩 20%（×0.8 → ×0.56）。
	// 以脚底 (0,0,-96) 为锚点做均匀缩放，保证脚不悬空、各部件仍相连。
	auto MakePart = [this](const TCHAR* Name,
		const FVector& Loc, const FVector& Scale, const FLinearColor& Color, bool bUpper = false) -> UStaticMeshComponent*
	{
		const FVector Anchor(0.f, 0.f, -96.f);
		const float Factor = bUpper ? (0.7f * 0.8f) : 0.7f;
		const FVector NewScale = Scale * Factor;
		const FVector NewLoc = Anchor + (Loc - Anchor) * Factor;

		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(GetCapsuleComponent());
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetRelativeLocation(NewLoc);
		Part->SetRelativeScale3D(NewScale);
		// 名字里带 Cyl 的部件用圆柱（躯干/四肢），其余用球（头/关节）
		const bool bUseCylinder = FString(Name).Contains(TEXT("Arm"))
			|| FString(Name).Contains(TEXT("Leg"))
			|| FString(Name).Contains(TEXT("Torso"))
			|| FString(Name).Contains(TEXT("Chest"))
			|| FString(Name).Contains(TEXT("Waist"));
		if (bUseCylinder && CylinderAsset.Succeeded())
		{
			Part->SetStaticMesh(CylinderAsset.Object);
		}
		else if (SphereAsset.Succeeded())
		{
			Part->SetStaticMesh(SphereAsset.Object);
		}
		else if (CubeAsset.Succeeded())
		{
			Part->SetStaticMesh(CubeAsset.Object);
		}
		if (PartMatAsset.Succeeded())
		{
			// ⚠️ 不要在构造函数里建 MID：它会被序列化进关卡 Actor，
			// 保存时报 "Illegal reference to private object: MaterialInstanceEditorOnlyData"。
			Part->SetMaterial(0, PartMatAsset.Object);
		}
		GeoParts.Add(Part);
		GeoPartColors.Add(Color);
		return Part;
	};

	// ── 方块机器人（11-01 改回几何体）──
	// 8 个基础体：躯干/头/双臂/双腿/胸灯/面罩。X=前，Y=左右，Z=上下；脚底在 -96。
	// 比例按胶囊 192 高来配：脚底 -96，头顶 +83 ≈ 179cm
	MakePart(TEXT("Torso"), FVector(0.f, 0.f, 4.f), FVector(0.45f, 0.32f, 0.90f), RobotColor, true);
	MakePart(TEXT("Head"), FVector(0.f, 0.f, 66.f), FVector(0.34f, 0.34f, 0.34f), RobotColor, true);
	MakePart(TEXT("ArmL"), FVector(0.f, -36.f, 1.f), FVector(0.14f, 0.14f, 0.60f), RobotColor, true);
	MakePart(TEXT("ArmR"), FVector(0.f, 36.f, 1.f), FVector(0.14f, 0.14f, 0.60f), RobotColor, true);
	MakePart(TEXT("LegL"), FVector(0.f, -16.f, -68.5f), FVector(0.16f, 0.16f, 0.55f), RobotColor);
	MakePart(TEXT("LegR"), FVector(0.f, 16.f, -68.5f), FVector(0.16f, 0.16f, 0.55f), RobotColor);
	MakePart(TEXT("ChestLight"), FVector(0.f, 0.f, 30.f), FVector(0.10f, 0.10f, 0.06f), JointColor, true);

	// 面罩：MakePart 默认会给球体，这里换成方块（贴在脸前方 X+）
	if (UStaticMeshComponent* Mask = MakePart(TEXT("Mask"), FVector(20.f, 0.f, 70.f),
		FVector(0.05f, 0.24f, 0.11f), JointColor, true))
	{
		if (CubeAsset.Succeeded())
		{
			Mask->SetStaticMesh(CubeAsset.Object);
		}
	}

	// 接触阴影：脚下圆盘，不透明深灰（不依赖半透明材质的参数名）
	ShadowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShadowMesh"));
	ShadowMesh->SetupAttachment(GetCapsuleComponent());
	ShadowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShadowMesh->SetCastShadow(false);
	ShadowMesh->SetRelativeLocation(FVector(0.f, 0.f, -95.f));
	ShadowMesh->SetRelativeScale3D(FVector(0.95f * 0.7f, 0.95f * 0.7f, 0.02f));
	if (SphereAsset.Succeeded())
	{
		ShadowMesh->SetStaticMesh(SphereAsset.Object);
	}
	if (PartMatAsset.Succeeded())
	{
		ShadowMesh->SetMaterial(0, PartMatAsset.Object);
	}
	} // if (!bUseSkeletalRobot)

	// 11-01 用户要求：腰间不再显示 3 个传递物（数量只保留在 HUD 文字里）

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 500.f, 0.f);
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// GAS：容器 + 属性集
	AbilitySystemComponent = CreateDefaultSubobject<UChuandiAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(false);

	AttributeSet = CreateDefaultSubobject<UChuandiAttributeSet>(TEXT("AttributeSet"));
	// 注意：这个模板函数不接受 TObjectPtr，必须传裸指针，否则推导失败
	AbilitySystemComponent->AddAttributeSetSubobject(AttributeSet.Get());
}

UAbilitySystemComponent* AChuandiPlayerCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AChuandiPlayerCharacter::BuildGeoMaterials()
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

	// 接触阴影：近黑
	if (ShadowMesh)
	{
		if (UMaterialInstanceDynamic* ShadowMID = ShadowMesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			ShadowMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.04f, 0.04f, 0.04f, 1.f));
		}
	}
}

void AChuandiPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	BuildGeoMaterials();

	if (!AbilitySystemComponent || !AttributeSet)
	{
		UE_LOG(LogChuandi, Error, TEXT("[GAS] ASC 或 AttributeSet 没创建出来"));
		return;
	}

	// 必须调，否则技能无法激活、属性不生效（04 §11 R8）
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (const UChuandiTuningData* Tuning = UChuandiTuningData::Get())
	{
		AttributeSet->SetMaxHealth(Tuning->PlayerMaxHealth);
		AttributeSet->SetHealth(Tuning->PlayerMaxHealth);
		GetCharacterMovement()->MaxWalkSpeed = Tuning->PlayerWalkSpeed;

		// 跳跃按"高 1.5m + 滞空 0.6s"反解重力与初速（07 §0），
		// 默认重力下这两个数不可能同时成立：h = v0·t/4, g = 2v0/t
		if (Tuning->JumpAirTime > 0.01f)
		{
			const float JumpZ = 4.f * Tuning->JumpHeight / Tuning->JumpAirTime;
			const float GravityZ = 2.f * JumpZ / Tuning->JumpAirTime;
			UCharacterMovementComponent* Movement = GetCharacterMovement();
			Movement->JumpZVelocity = JumpZ;
			Movement->GravityScale = GravityZ / 980.f;
		}
	}

	AttributeSet->OnDeath.AddUObject(this, &AChuandiPlayerCharacter::HandleDeath);

	// 减速 GE 改的是 MoveSpeed 属性，这里把它落到实际移动速度上
	AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UChuandiAttributeSet::GetMoveSpeedAttribute())
		.AddUObject(this, &AChuandiPlayerCharacter::OnMoveSpeedChanged);

	// 授予技能（D5：普攻 + 弹反 + 布阱）
	AbilitySystemComponent->GiveAbility(
		FGameplayAbilitySpec(UGA_PlayerAttack::StaticClass(), 1, INDEX_NONE, this));
	AbilitySystemComponent->GiveAbility(
		FGameplayAbilitySpec(UGA_PlayerParry::StaticClass(), 1, INDEX_NONE, this));
	AbilitySystemComponent->GiveAbility(
		FGameplayAbilitySpec(UGA_PlayerPlaceTrap::StaticClass(), 1, INDEX_NONE, this));
	AbilitySystemComponent->GiveAbility(
		FGameplayAbilitySpec(UGA_PlayerDodge::StaticClass(), 1, INDEX_NONE, this));
	AbilitySystemComponent->GiveAbility(
		FGameplayAbilitySpec(UGA_PlayerSkill::StaticClass(), 1, INDEX_NONE, this));

	UE_LOG(LogChuandi, Log, TEXT("[GAS] ASC 就绪: %s  HP=%.1f/%.1f"),
		*GetNameSafe(this), AttributeSet->GetHealth(), AttributeSet->GetMaxHealth());
}

void AChuandiPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogChuandi, Error, TEXT("[Input] 没拿到 UEnhancedInputComponent。检查 Project Settings > Input > Default Classes。"));
		return;
	}

	MoveAction = LoadInputAction(MoveActionPath);
	LookAction = LoadInputAction(LookActionPath);
	JumpAction = LoadInputAction(JumpActionPath);
	AttackAction = LoadInputAction(AttackActionPath);
	ParryAction = LoadInputAction(ParryActionPath);
	TrapAction = LoadInputAction(TrapActionPath);
	DodgeAction = LoadInputAction(DodgeActionPath);
	SkillAction = LoadInputAction(SkillActionPath);

	if (MoveAction)
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AChuandiPlayerCharacter::Move);
	}
	if (LookAction)
	{
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AChuandiPlayerCharacter::Look);
	}
	if (JumpAction)
	{
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}
	if (AttackAction)
	{
		EIC->BindAction(AttackAction, ETriggerEvent::Started, this, &AChuandiPlayerCharacter::OnAttackPressed);
	}
	if (ParryAction)
	{
		EIC->BindAction(ParryAction, ETriggerEvent::Started, this, &AChuandiPlayerCharacter::OnParryPressed);
	}
	if (TrapAction)
	{
		EIC->BindAction(TrapAction, ETriggerEvent::Started, this, &AChuandiPlayerCharacter::OnTrapPressed);
		EIC->BindAction(TrapAction, ETriggerEvent::Completed, this, &AChuandiPlayerCharacter::OnTrapReleased);
	}
	if (DodgeAction)
	{
		EIC->BindAction(DodgeAction, ETriggerEvent::Started, this, &AChuandiPlayerCharacter::OnDodgePressed);
	}
	if (SkillAction)
	{
		EIC->BindAction(SkillAction, ETriggerEvent::Started, this, &AChuandiPlayerCharacter::OnSkillPressed);
	}

	// 取消键：X。Enhanced Input 里没有 IA_Cancel 资产，直接用传统按键绑定，
	// 与 Enhanced Input 并存没问题；以后要做键位重映射再换成 IA_Cancel。
	if (PlayerInputComponent)
	{
		PlayerInputComponent->BindKey(EKeys::X, IE_Pressed, this, &AChuandiPlayerCharacter::OnCancelPressed);

		// 重开：调试期最常用的键。死一次就重跑，不用停 PIE
		PlayerInputComponent->BindKey(EKeys::R, IE_Pressed, this, &AChuandiPlayerCharacter::OnRestartPressed);

		// 柱子交互：F（10-29）。同样走传统按键，避免再建一个 IA 资产。
		PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &AChuandiPlayerCharacter::OnInteractPressed);

		// 录 demo：F1 隐藏/显示 HUD 上的开发信息（状态机名、怀疑度条）
		PlayerInputComponent->BindKey(EKeys::F1, IE_Pressed, this, &AChuandiPlayerCharacter::OnToggleDebugHUDPressed);
	}
}

bool AChuandiPlayerCharacter::IsActionLocked() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < ActionLockUntil;
}

void AChuandiPlayerCharacter::LockActions(float Seconds)
{
	if (const UWorld* World = GetWorld())
	{
		ActionLockUntil = FMath::Max(ActionLockUntil, World->GetTimeSeconds() + Seconds);
	}
}

void AChuandiPlayerCharacter::StartDash(const FVector& Direction, float Duration, float Speed, float InvincibleTime)
{
	DashDirection = Direction.GetSafeNormal2D();
	if (DashDirection.IsNearlyZero())
	{
		DashDirection = GetActorForwardVector().GetSafeNormal2D();
	}

	DashTimeLeft = Duration;
	DashSpeed = Speed;

	// 闪避动画
	if (AnimDash)
	{
		PlayActionAnim(AnimDash, Duration);
	}

	// 冷却起点（HUD 扫回用）
	if (const UWorld* World = GetWorld())
	{
		const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
		DodgeCooldownEndTime = World->GetTimeSeconds() + (Tuning ? Tuning->DodgeCooldown : 0.8f);
	}

	// 无敌帧：用 loose tag + 定时移除（不占 GE 时长逻辑，够用且不会漏）
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AddLooseGameplayTag(ChuandiTags::State_Invincible.GetTag());

		FTimerHandle Handle;
		TWeakObjectPtr<UChuandiAbilitySystemComponent> WeakASC(AbilitySystemComponent);
		GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakASC]()
		{
			if (UChuandiAbilitySystemComponent* ASC = WeakASC.Get())
			{
				ASC->RemoveLooseGameplayTag(ChuandiTags::State_Invincible.GetTag());
			}
		}), InvincibleTime, false);
	}

	UE_LOG(LogChuandi, Log, TEXT("[Dodge] 冲刺 %.0fcm（%.2fs，无敌 %.2fs）"),
		Speed * Duration, Duration, InvincibleTime);
}

void AChuandiPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bAimingTrap)
	{
		UpdateTrapAim();
	}

	UpdateRobotAnim(DeltaSeconds);

	// 柱子交互：准星锁敌 + 跳跃弧线（10-29）
	UpdatePillarFocus();
	UpdateClimb(DeltaSeconds);

	// 镜头轻推：指数衰减，SpringArm 后推 + 略抬
	if (CameraBoom)
	{
		CameraKickStrength = FMath::FInterpTo(CameraKickStrength, 0.f, DeltaSeconds, 12.f);
		CameraBoom->SocketOffset = FVector(-CameraKickStrength, 0.f, CameraKickStrength * 0.35f);
	}

	// 待机呼吸 ±2%（04 §4）：只改身躯，不影响胶囊碰撞
	// 呼吸缩放已移除：改用骨骼动画后整体缩放会和动画叠加，看起来在抽搐

	if (DashTimeLeft <= 0.f)
	{
		return;
	}

	// 冲刺期间把速度拉到目标值，并强制朝冲刺方向输入
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = DashSpeed;
	}
	AddMovementInput(DashDirection, 1.f);

	DashTimeLeft -= DeltaSeconds;
	if (DashTimeLeft <= 0.f)
	{
		// 恢复成属性里的移速（减速 GE 也走这里）
		if (AttributeSet && GetCharacterMovement())
		{
			GetCharacterMovement()->MaxWalkSpeed = AttributeSet->GetMoveSpeed();
		}
	}
}

void AChuandiPlayerCharacter::OnSkillPressed()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (AbilitySystemComponent->TryActivateAbilityByClass(UGA_PlayerSkill::StaticClass()))
	{
		const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
		if (AnimAttacks.Num() > 0)
		{
			PlayActionAnim(AnimAttacks.Last(), 0.f);
		}
		const float Cooldown = Tuning ? Tuning->PlayerSkillCooldown : 8.f;
		if (const UWorld* World = GetWorld())
		{
			SkillCooldownEndTime = World->GetTimeSeconds() + Cooldown;
		}
	}
}

float AChuandiPlayerCharacter::GetParryCooldownRatio() const
{
	const UWorld* World = GetWorld();
	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float Total = Tuning ? Tuning->ParryCooldown : 1.2f;
	if (!World || Total <= 0.f)
	{
		return 0.f;
	}
	const float Left = ParryCooldownEndTime - World->GetTimeSeconds();
	return FMath::Clamp(Left / Total, 0.f, 1.f);
}

float AChuandiPlayerCharacter::GetDodgeCooldownRatio() const
{
	const UWorld* World = GetWorld();
	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float Total = Tuning ? Tuning->DodgeCooldown : 0.8f;
	if (!World || Total <= 0.f)
	{
		return 0.f;
	}
	const float Left = DodgeCooldownEndTime - World->GetTimeSeconds();
	return FMath::Clamp(Left / Total, 0.f, 1.f);
}

float AChuandiPlayerCharacter::GetSkillCooldownRatio() const
{
	const UWorld* World = GetWorld();
	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float Total = Tuning ? Tuning->PlayerSkillCooldown : 8.f;
	if (!World || Total <= 0.f)
	{
		return 0.f;
	}
	const float Left = SkillCooldownEndTime - World->GetTimeSeconds();
	return FMath::Clamp(Left / Total, 0.f, 1.f);
}

void AChuandiPlayerCharacter::OnDodgePressed()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->TryActivateAbilityByClass(UGA_PlayerDodge::StaticClass());
}

void AChuandiPlayerCharacter::OnAttackPressed()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	const bool bActivated = AbilitySystemComponent->TryActivateAbilityByClass(UGA_PlayerAttack::StaticClass());
	if (!bActivated)
	{
		UE_LOG(LogChuandi, Warning, TEXT("[Attack] 普攻激活失败（技能没授予 / 冷却中 / 被 tag 阻挡）"));
		return;
	}

	// 普攻动作：三段攻击动画随机播一段
	if (AnimAttacks.Num() > 0)
	{
		const int32 Idx = FMath::RandRange(0, AnimAttacks.Num() - 1);
		PlayActionAnim(AnimAttacks[Idx], 0.f);   // 0 = 用动画自身时长
	}
}

void AChuandiPlayerCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!GetController())
	{
		return;
	}

	const FRotator YawRotation(0.f, GetController()->GetControlRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, Axis.Y);
	AddMovementInput(Right, Axis.X);
}

void AChuandiPlayerCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AChuandiPlayerCharacter::OnParryPressed()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	// 冷却中会直接失败（打印在 GA_PlayerParry 里）
	AbilitySystemComponent->TryActivateAbilityByClass(UGA_PlayerParry::StaticClass());

	// 冷却从"按下"起算（07 §1.2 / C-2），HUD 用它画扫回
	if (const UWorld* World = GetWorld())
	{
		const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
		ParryCooldownEndTime = World->GetTimeSeconds() + (Tuning ? Tuning->ParryCooldown : 1.2f);

		// 结算统计：冷却中按的也算"尝试"（否则成功率会虚高）
		if (AChuandiGameMode* GM = Cast<AChuandiGameMode>(UGameplayStatics::GetGameMode(this)))
		{
			GM->RegisterParryAttempt();
		}
	}
}

void AChuandiPlayerCharacter::OnTrapPressed()
{
	// 07 §2.1：长按开始选位（不消耗传递物、不进入施法）
	if (bAimingTrap)
	{
		return;
	}

	bAimingTrap = true;
	bPendingTrapValid = false;

	UWorld* World = GetWorld();
	if (World && !TrapIndicator)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		TrapIndicator = World->SpawnActor<ATrapIndicatorActor>(
			ATrapIndicatorActor::StaticClass(), GetActorLocation(), FRotator::ZeroRotator, Params);
	}

	UpdateTrapAim();
	UE_LOG(LogChuandi, Verbose, TEXT("[Trap] 开始选位"));
}

void AChuandiPlayerCharacter::UpdateTrapAim()
{
	if (!bAimingTrap)
	{
		return;
	}

	FVector AimPoint = GetActorLocation();
	bool bValid = false;

	// 即使求交失败也要更新指示器位置，否则它会停在出生点（表现为"悬浮在半空、白色"）
	ComputeTrapAimPoint(AimPoint, bValid);

	PendingTrapLocation = AimPoint;
	bPendingTrapValid = bValid;

	if (TrapIndicator)
	{
		const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
		const float Radius = Tuning ? Tuning->TrapTriggerRadius : 150.f;
		TrapIndicator->SetTarget(AimPoint, bValid, Radius);
	}

	// 诊断：每秒一条，看清落点在哪儿、为什么合法/不合法
	AimLogAccum += GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	if (AimLogAccum >= 1.f)
	{
		AimLogAccum = 0.f;
		UE_LOG(LogChuandi, Log, TEXT("[Trap] 选位 落点=(%.0f, %.0f, %.0f) 距离=%.0f 合法=%d"),
			AimPoint.X, AimPoint.Y, AimPoint.Z,
			FVector::Dist2D(AimPoint, GetActorLocation()), bValid ? 1 : 0);
	}
}

bool AChuandiPlayerCharacter::ComputeTrapAimPoint(FVector& OutPoint, bool& bOutValid) const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!World || !PC)
	{
		return false;
	}

	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float MaxDistance = Tuning ? Tuning->TrapMaxPlaceDistance : 500.f;

	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

	// 落点 = 准星方向的**水平投影** × 距离上限（5m），再把 Z 压到地面。
	// 为什么不用"准星与地面的交点"：第三人称相机在角色后上方，屏幕中心稍微朝下就会
	// 命中脚边地面，实测落点永远在 90cm 左右，永远触发不了超距、也放不到想放的位置。
	FVector Direction = ViewRotation.Vector();
	Direction.Z = 0.f;
	if (Direction.IsNearlyZero())
	{
		Direction = GetActorForwardVector();
	}
	Direction.Normalize();

	// 07 §2.1：距离上限 5m
	const FVector FlatPoint = GetActorLocation() + Direction * MaxDistance;

	// 向下找地面（唯一还需要射线的步骤）；探不到就退化成玩家脚底高度
	FHitResult GroundHit;
	FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(TrapAimGround), false, this);
	const float FallbackGroundZ = GetActorLocation().Z
		- (GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f);

	float GroundZ = FallbackGroundZ;
	bool bHitGround = false;
	if (World->LineTraceSingleByChannel(GroundHit,
		FlatPoint + FVector(0.f, 0.f, 400.f), FlatPoint - FVector(0.f, 0.f, 1000.f),
		ECC_Visibility, GroundParams))
	{
		GroundZ = GroundHit.ImpactPoint.Z;
		bHitGround = true;
	}

	OutPoint = FVector(FlatPoint.X, FlatPoint.Y, GroundZ);

	// 07 §4.4：不可放在碎石上（用距离查，不需要射线）
	bool bOnCover = false;
	for (TActorIterator<AChuandiCoverActor> It(World); It; ++It)
	{
		if (FVector::Dist2D(It->GetActorLocation(), OutPoint) < 120.f)
		{
			bOnCover = true;
			break;
		}
	}

	// 距离恒定等于上限，所以"超距"不可达；贴地由"探到地面 / 平地兜底"保证
	// 10-29：场景取消空气墙、可无限大 → 不再要求落点在 40×40 内（原 07 §4.1 的界内判定已删），
	// 只剩"不可放在碎石上"这一条硬约束，否则玩家走到原场地边界外就再也放不了陷阱。
	bOutValid = !bOnCover;
	return true;
}

void AChuandiPlayerCharacter::OnTrapReleased()
{
	if (!bAimingTrap)
	{
		return;
	}

	bAimingTrap = false;

	if (TrapIndicator)
	{
		TrapIndicator->Destroy();
		TrapIndicator = nullptr;
	}

	// 07 §2.2：松开在不可放置处 = 取消，返还传递物（这里根本没消耗）
	if (!bPendingTrapValid)
	{
		UE_LOG(LogChuandi, Log, TEXT("[Trap] 取消：落点不合法（在碎石上）"));
		return;
	}

	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->TryActivateAbilityByClass(UGA_PlayerPlaceTrap::StaticClass());
}

bool AChuandiPlayerCharacter::ConsumePendingTrapLocation(FVector& OutLocation)
{
	if (!bPendingTrapValid)
	{
		return false;
	}

	OutLocation = PendingTrapLocation;
	bPendingTrapValid = false;
	return true;
}

void AChuandiPlayerCharacter::OnCancelPressed()
{
	if (!bAimingTrap)
	{
		return;
	}

	bAimingTrap = false;
	bPendingTrapValid = false;
	PendingTrapLocation = FVector::ZeroVector;

	if (TrapIndicator)
	{
		TrapIndicator->Destroy();
		TrapIndicator = nullptr;
	}

	// 传递物从未被消耗，所以"取消返还"在这里是天然成立的
	UE_LOG(LogChuandi, Log, TEXT("[Trap] 取消（X），未消耗传递物"));
}

void AChuandiPlayerCharacter::OnRestartPressed()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 重载当前关卡：Boss 血量、破防条、传递物、碎石全部回到初始
	const FString LevelName = UGameplayStatics::GetCurrentLevelName(this, /*bRemovePrefixString=*/true);
	UE_LOG(LogChuandi, Log, TEXT("[GM] 重开：%s"), *LevelName);
	UGameplayStatics::OpenLevel(this, FName(*LevelName));
}

void AChuandiPlayerCharacter::PlayActionAnim(UAnimSequence* Seq, float Duration)
{
	if (!RobotMesh || !Seq)
	{
		return;
	}
	ActionAnim = Seq;
	ActionAnimTimer = Duration > 0.f ? Duration : Seq->GetPlayLength();
	RobotMesh->PlayAnimation(Seq, /*bLooping=*/false);

	// 攻击无前摇（07 §1.5）：伤害在按下瞬间就结算，动画也不该先摆 0.25s 起手，
	// 否则"伤害已到、刀还没挥"看起来像有前摇。跳过起手段 + 提速。
	RobotMesh->SetPlayRate(1.35f);
	RobotMesh->SetPosition(Seq->GetPlayLength() * 0.30f, /*bFireNotifies=*/false);

	CurrentAnim = Seq;
}

void AChuandiPlayerCharacter::BeginParryRoot()
{
	if (bParryRooted)
	{
		return;
	}
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		SavedMoveMode = Movement->MovementMode;
		Movement->StopMovementImmediately();
		Movement->DisableMovement();   // MOVE_None：弹反期间彻底不能移动
		bParryRooted = true;
	}
}

void AChuandiPlayerCharacter::EndParryRoot()
{
	if (!bParryRooted)
	{
		return;
	}
	bParryRooted = false;

	// 弹反窗口结束时不能顺手解掉"牵引禁锢"：谁还在定身谁说了算
	if (!bForceRooted)
	{
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->SetMovementMode(SavedMoveMode == MOVE_None ? MOVE_Walking : static_cast<EMovementMode>(SavedMoveMode));
		}
	}
}

void AChuandiPlayerCharacter::BeginForceRoot()
{
	if (bForceRooted)
	{
		return;
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		if (!bParryRooted)
		{
			SavedMoveMode = Movement->MovementMode;
		}
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
	bForceRooted = true;
}

void AChuandiPlayerCharacter::EndForceRoot()
{
	if (!bForceRooted)
	{
		return;
	}
	bForceRooted = false;

	if (!bParryRooted)
	{
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->SetMovementMode(SavedMoveMode == MOVE_None ? MOVE_Walking : static_cast<EMovementMode>(SavedMoveMode));
		}
	}
}

void AChuandiPlayerCharacter::OnToggleDebugHUDPressed()
{
	ToggleDebugHUD();
}

void AChuandiPlayerCharacter::ToggleDebugHUD()
{
	bShowDebugHUD = !bShowDebugHUD;
	UE_LOG(LogChuandi, Log, TEXT("[HUD] 调试信息 %s（录 demo 时建议关闭）"),
		bShowDebugHUD ? TEXT("显示") : TEXT("隐藏"));
}

void AChuandiPlayerCharacter::PlayCameraKick(float Strength)
{
	CameraKickStrength = FMath::Max(CameraKickStrength, Strength);
}

void AChuandiPlayerCharacter::UpdateRobotAnim(float DeltaSeconds)
{
	if (!RobotMesh)
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
	else if (GetCharacterMovement() && GetCharacterMovement()->IsFalling())
	{
		Want = AnimJump;
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
		RobotMesh->PlayAnimation(Want, bLoop);
		RobotMesh->SetPlayRate(1.f);   // 动作动画才提速，循环动作保持原速
	}
}

bool AChuandiPlayerCharacter::ConsumeTrapCharge()
{
	if (TrapCharges <= 0)
	{
		return false;
	}

	--TrapCharges;
	return true;
}

void AChuandiPlayerCharacter::OnMoveSpeedChanged(const FOnAttributeChangeData& Data)
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = Data.NewValue;
	}
}

void AChuandiPlayerCharacter::HandleDeath(AActor* DeadActor)
{
	UE_LOG(LogChuandi, Log, TEXT("[Player] 死亡"));

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->DisableMovement();
	}

	// 死亡 tag：伤害入口会忽略，后续技能也激活不了
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAllAbilities();
		AbilitySystemComponent->AddLooseGameplayTag(ChuandiTags::State_Dead.GetTag());
	}

	if (AChuandiGameMode* GM = Cast<AChuandiGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->NotifyPlayerDied();
	}
}

// ─────────────────────────────────────────────────────────────
// 柱子交互（10-29）：柱子 3m+，跳不上去，只能对准按 F 上/飞跃
// ─────────────────────────────────────────────────────────────

void AChuandiPlayerCharacter::UpdatePillarFocus()
{
	FocusedPillar = nullptr;

	if (bClimbing)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!World || !PC)
	{
		return;
	}

	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float Range = Tuning ? Tuning->InteractAimRange : 3000.f;

	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

	// 只用准星射线判定"对准"：射线打到柱体就算，不额外卡距离/角度
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PillarFocus), false, this);
	if (World->LineTraceSingleByChannel(Hit, ViewLocation,
		ViewLocation + ViewRotation.Vector() * Range, ECC_Visibility, Params))
	{
		FocusedPillar = Cast<AChuandiCoverActor>(Hit.GetActor());
	}
}

AChuandiCoverActor* AChuandiPlayerCharacter::GetStandingPillar() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const float HalfHeight = GetCapsuleComponent()
		? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
	const FVector Start = GetActorLocation();
	const FVector End = Start + FVector(0.f, 0.f, -(HalfHeight + 30.f));

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PillarStand), false, this);
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return Cast<AChuandiCoverActor>(Hit.GetActor());
	}
	return nullptr;
}

bool AChuandiPlayerCharacter::IsStandingOnPillar() const
{
	return GetStandingPillar() != nullptr;
}

void AChuandiPlayerCharacter::OnInteractPressed()
{
	if (bClimbing || IsActionLocked())
	{
		return;
	}

	AChuandiCoverActor* Target = FocusedPillar.Get();
	if (!IsValid(Target))
	{
		return;
	}

	// 已经站在这一根上 → 按 F 没有意义（避免原地"弹一下"）
	if (IsStandingOnPillar())
	{
		FHitResult Hit;
		const float HalfHeight = GetCapsuleComponent()
			? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(PillarStand), false, this);
		if (GetWorld() && GetWorld()->LineTraceSingleByChannel(Hit,
			GetActorLocation(), GetActorLocation() + FVector(0.f, 0.f, -(HalfHeight + 30.f)),
			ECC_Visibility, Params) && Cast<AChuandiCoverActor>(Hit.GetActor()) == Target)
		{
			return;
		}
	}

	const float HalfHeight = GetCapsuleComponent()
		? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;

	const FVector Top = Target->GetStandTopLocation();
	ClimbStart = GetActorLocation();
	ClimbTarget = FVector(Top.X, Top.Y, Top.Z + HalfHeight + 2.f);
	ClimbElapsed = 0.f;
	bClimbing = true;

	// 跳跃期间接管位移：关掉移动模式，避免输入和插值互相打架
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->SetMovementMode(MOVE_None);
	}

	UE_LOG(LogChuandi, Log, TEXT("[Interact] F → 跳向柱子 %s（柱高 %.0fcm）"),
		*GetNameSafe(Target), Target->GetRubbleHeight());
}

void AChuandiPlayerCharacter::UpdateClimb(float DeltaSeconds)
{
	if (!bClimbing)
	{
		return;
	}

	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float Duration = FMath::Max(0.05f, Tuning ? Tuning->ClimbDuration : 0.35f);
	const float ArcHeight = Tuning ? Tuning->ClimbArcHeight : 120.f;

	ClimbElapsed += DeltaSeconds;
	const float T = FMath::Clamp(ClimbElapsed / Duration, 0.f, 1.f);

	FVector Pos = FMath::Lerp(ClimbStart, ClimbTarget, T);
	Pos.Z += FMath::Sin(T * PI) * ArcHeight;
	SetActorLocation(Pos, /*bSweep=*/false);

	if (T >= 1.f)
	{
		bClimbing = false;
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			Move->SetMovementMode(MOVE_Walking);
		}
	}
}
