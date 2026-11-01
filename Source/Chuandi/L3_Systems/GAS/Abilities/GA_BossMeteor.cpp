#include "L3_Systems/GAS/Abilities/GA_BossMeteor.h"

#include "L0_Core/ChuandiLog.h"
#include "L1_Framework/ChuandiTuningData.h"
#include "L2_Entities/Boss/ChuandiBossCharacter.h"
#include "L2_Entities/World/CoverActor.h"
#include "L2_Entities/World/TelegraphActor.h"
#include "L3_Systems/BossStateMachineComponent.h"
#include "L3_Systems/BreakMeterComponent.h"
#include "L3_Systems/GAS/ChuandiAbilitySystemComponent.h"
#include "L3_Systems/GAS/ChuandiAttributeSet.h"
#include "L3_Systems/GAS/ChuandiCombatLibrary.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

UGA_BossMeteor::UGA_BossMeteor()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
}

void UGA_BossMeteor::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UWorld* World = GetWorld();
	AActor* Boss = GetAvatarActorFromActorInfo();
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!World || !Boss || !Player)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	CachedRadius = Tuning ? Tuning->MeteorRadius : 400.f;
	CachedDamage = Tuning ? Tuning->MeteorDamage : 15.f;

	// ── 阶段判定（02 §6.4）：80% 加密一档（7 圈 / 2.8s）、40% 再加密（8 圈 / 2.5s）──
	float HPRatio = 1.f;
	if (const AChuandiBossCharacter* BossChar = Cast<AChuandiBossCharacter>(Boss))
	{
		if (const UChuandiAttributeSet* BossAttr = BossChar->GetChuandiAttributeSet())
		{
			const float MaxHP = FMath::Max(1.f, BossAttr->GetMaxHealth());
			HPRatio = BossAttr->GetHealth() / MaxHP;
		}
	}

	const float DenseThreshold = Tuning ? Tuning->MeteorDenseThreshold : 0.4f;
	const float MidThreshold = Tuning ? Tuning->MeteorDense2Threshold : 0.8f;
	const bool bDensePhase = HPRatio <= DenseThreshold;        // ≤40%
	const bool bMidPhase = !bDensePhase && HPRatio <= MidThreshold;  // ≤80%

	const float CircleLifetime = bDensePhase
		? (Tuning ? Tuning->MeteorDenseWarningTime : 2.5f)
		: (bMidPhase
			? (Tuning ? Tuning->MeteorDense2WarningTime : 2.8f)
			: (Tuning ? Tuning->MeteorWarningTime : 3.0f));
	const int32 CircleCount = bDensePhase
		? (Tuning ? Tuning->MeteorDenseCircleCount : 8)
		: (bMidPhase
			? (Tuning ? Tuning->MeteorDense2CircleCount : 7)
			: (Tuning ? Tuning->MeteorCircleCount : 6));
	const float MinSpacing = Tuning ? Tuning->MeteorCircleSpacing : 800.f;

	// ── 落点：全屏 6 个，互相间距 >= 6m（交接 10-28 17:22）──
	// 第一个一定在玩家脚下附近，保证"就近选一个"永远有可选目标；
	// 其余在场地里撒开，形成"选哪个"的判断。
	// 黄圈要画在地上，不是画在人身上：玩家的 ActorLocation 在胶囊中心（离地约 1m），
	// 直接拿它当落点会让圈悬在半空。
	const FVector PlayerActorLoc = Player->GetActorLocation();
	float PlayerGroundZ = 0.f;
	{
		FHitResult GroundHit;
		const FVector TraceStart = PlayerActorLoc + FVector(0.f, 0.f, 200.f);
		const FVector TraceEnd = PlayerActorLoc - FVector(0.f, 0.f, 500.f);
		FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(MeteorGround), false, Player);
		if (World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, TraceParams))
		{
			PlayerGroundZ = GroundHit.ImpactPoint.Z;
		}
		else
		{
			// 兜底：用胶囊半高反推地面（玩家胶囊半高 96）
			PlayerGroundZ = PlayerActorLoc.Z - 96.f;
		}
	}
	const FVector PlayerLoc(PlayerActorLoc.X, PlayerActorLoc.Y, PlayerGroundZ + 2.f);

	// ── 落点：以玩家为圆心撒圈（10-28 20:12 用户要求）──
	// 10-29 起场景无空气墙、可无限大 → **不再做"是否在场地内"的重抽**，
	// 只保留"两两间距 ≥ MinSpacing"这一条（避免相邻圈边缘重叠、判定归属不明）。
	// 半径随圈数放大，保持"圈覆盖面积占比"不变：6 圈 = 13m，8 圈 ≈ 15m，
	// 否则 8 个圈挤在 13m 里会把整块地铺满，玩家几乎永远站在圈内、弹反退化成"到点按 Q"。
	ImpactPoints.Reset();
	const float SpawnRadius = 1300.f * FMath::Sqrt(static_cast<float>(CircleCount) / 6.f);

	// 随机重抽：凑不满时逐级放宽间距（8 圈在 15m 内按 8m 间距撒，直接抽会经常抽不满）
	for (float Spacing = MinSpacing; Spacing >= MinSpacing * 0.7f; Spacing *= 0.85f)
	{
		int32 Guard = 0;
		while (ImpactPoints.Num() < CircleCount && Guard++ < 600)
		{
			// 圆内均匀采样：sqrt 保证不往圆心堆
			const float Angle = FMath::FRandRange(0.f, 2.f * PI);
			const float Dist = SpawnRadius * FMath::Sqrt(FMath::FRand());

			const FVector Candidate(
				PlayerLoc.X + FMath::Cos(Angle) * Dist,
				PlayerLoc.Y + FMath::Sin(Angle) * Dist,
				PlayerLoc.Z);

			bool bTooClose = false;
			for (const FVector& Existing : ImpactPoints)
			{
				if (FVector::Dist2D(Existing, Candidate) < Spacing)
				{
					bTooClose = true;
					break;
				}
			}
			if (bTooClose)
			{
				continue;
			}

			ImpactPoints.Add(Candidate);
		}

		if (ImpactPoints.Num() >= CircleCount)
		{
			break;
		}
	}

	// 兜底：运气太差时把剩下的补在玩家周围（保证一定有圈可弹）
	while (ImpactPoints.Num() < CircleCount)
	{
		const float Angle = 2.f * PI * ImpactPoints.Num() / CircleCount;
		ImpactPoints.Add(FVector(
			PlayerLoc.X + FMath::Cos(Angle) * 500.f,
			PlayerLoc.Y + FMath::Sin(Angle) * 500.f,
			PlayerLoc.Z));
	}

	Telegraphs.Reset();
	for (const FVector& Point : ImpactPoints)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ATelegraphActor* Telegraph = World->SpawnActor<ATelegraphActor>(
			ATelegraphActor::StaticClass(), Point, FRotator::ZeroRotator, SpawnParams);
		if (Telegraph)
		{
			Telegraph->PlayWarning(CircleLifetime, CachedRadius, FLinearColor(0.95f, 0.75f, 0.15f, 1.f));
			Telegraphs.Add(Telegraph);
		}
	}

	UE_LOG(LogChuandi, Log,
		TEXT("[Meteor] 施法开始：%d 个黄圈（%s），存在 %.1fs，半径 %.0f（跑进任意圈内按 Q 弹反）"),
		ImpactPoints.Num(),
		bDensePhase ? TEXT("40% 加密期") : (bMidPhase ? TEXT("80% 加密期") : TEXT("常态")),
		CircleLifetime, CachedRadius);

	UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, CircleLifetime);
	DelayTask->OnFinish.AddDynamic(this, &UGA_BossMeteor::OnMeteorLand);
	DelayTask->ReadyForActivation();
}

namespace
{
	/** 落点附近有人就不该生成实心掩体，否则会把角色夹在石头里（踩过一次：玩家卡死不能动） */
	bool IsPointOccupiedByPawn(const UWorld* World, const FVector& Point, float Radius)
	{
		if (!World)
		{
			return false;
		}

		TArray<AActor*> Pawns;
		UGameplayStatics::GetAllActorsOfClass(World, APawn::StaticClass(), Pawns);

		for (const AActor* Pawn : Pawns)
		{
			if (Pawn && FVector::Dist2D(Pawn->GetActorLocation(), Point) <= Radius)
			{
				return true;
			}
		}

		return false;
	}

	/** 在指定 XY 生成一根碎石柱（自动按上限清最旧的）。成功返回 true */
	bool SpawnRubbleAt(UWorld* World, AActor* Boss, const FVector& FlatPoint, const UChuandiTuningData* Tuning)
	{
		if (!World || !Tuning)
		{
			return false;
		}

		FHitResult GroundHit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MeteorGround), false, Boss);
		if (!World->LineTraceSingleByChannel(GroundHit, FlatPoint + FVector(0.f, 0.f, 600.f),
			FlatPoint - FVector(0.f, 0.f, 800.f), ECC_Visibility, Params))
		{
			return false;
		}

		TArray<AChuandiCoverActor*> Existing;
		for (TActorIterator<AChuandiCoverActor> It(World); It; ++It)
		{
			Existing.Add(*It);
		}
		if (Existing.Num() >= Tuning->CoverMaxCount)
		{
			Existing.Sort([](const AChuandiCoverActor& A, const AChuandiCoverActor& B)
			{
				return A.GetSpawnTime() < B.GetSpawnTime();
			});
			Existing[0]->Destroy();
		}

		const float Height = FMath::FRandRange(Tuning->RubbleHeightMin, Tuning->RubbleHeightMax);
		const float Footprint = FMath::FRandRange(Tuning->RubbleFootprintMin, Tuning->RubbleFootprintMax);
		const float Yaw = FMath::FRandRange(0.f, 360.f);

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AChuandiCoverActor* Cover = World->SpawnActor<AChuandiCoverActor>(
			AChuandiCoverActor::StaticClass(),
			FVector(FlatPoint.X, FlatPoint.Y, GroundHit.ImpactPoint.Z + Height * 0.5f),
			FRotator(0.f, Yaw, 0.f), SpawnParams);
		if (!Cover)
		{
			return false;
		}

		Cover->ConfigureRubble(Height, Footprint);
		return true;
	}

	/** 落地判定的三种结果 */
	enum class EMeteorOutcome : uint8
	{
		Parried,      // 圈内 + 弹反窗口开：全部消失，+40 条
		FailedInRing, // 圈内但没弹：只砸最近那个（坑 + 15 伤）
		Outside       // 完全不入圈：全屏 15 伤，不生成坑
	};
}

void UGA_BossMeteor::OnMeteorLand()
{
	UWorld* World = GetWorld();
	AActor* Boss = GetAvatarActorFromActorInfo();
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();

	// ── 1. 玩家在哪个圈里（取最近的那个）──
	int32 NearestIndex = INDEX_NONE;
	float NearestDist = TNumericLimits<float>::Max();
	if (Player)
	{
		for (int32 i = 0; i < ImpactPoints.Num(); ++i)
		{
			const float D = FVector::Dist2D(Player->GetActorLocation(), ImpactPoints[i]);
			if (D <= CachedRadius && D < NearestDist)
			{
				NearestDist = D;
				NearestIndex = i;
			}
		}
	}

	// ── 2. 圈内 + 窗口开 = 弹反成功 ──
	bool bParried = false;
	if (World && Player && NearestIndex != INDEX_NONE)
	{
		if (UChuandiAbilitySystemComponent* PlayerASC = Cast<UChuandiAbilitySystemComponent>(
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player)))
		{
			if (PlayerASC->IsParryWindowOpen(World->GetTimeSeconds()))
			{
				PlayerASC->ConsumeParryWindow();
				bParried = true;

				if (AChuandiBossCharacter* BossChar = Cast<AChuandiBossCharacter>(Boss))
				{
					BossChar->FlashColor(FLinearColor(1.f, 0.83f, 0.28f, 1.f), 0.2f);
					if (UBreakMeterComponent* Meter = BossChar->GetBreakMeter())
					{
						Meter->AddGauge(Tuning ? Tuning->BreakGaugeOnParry : 40.f);
					}
				}

				UChuandiCombatLibrary::HitStop(World, 0.06f, 0.05f);
				UE_LOG(LogChuandi, Log, TEXT("[Meteor] 弹反成功（在圈内），整轮伤害不结算，破防 +%.0f"),
					Tuning ? Tuning->BreakGaugeOnParry : 40.f);
			}
		}
	}

	const EMeteorOutcome Outcome = bParried
		? EMeteorOutcome::Parried
		: (NearestIndex != INDEX_NONE ? EMeteorOutcome::FailedInRing : EMeteorOutcome::Outside);

	// ── 3. 伤害结算 ──
	if (World && Boss && Player)
	{
		if (Outcome == EMeteorOutcome::Outside)
		{
			UChuandiCombatLibrary::ApplyDamage(Boss, Player, CachedDamage);
			UE_LOG(LogChuandi, Log, TEXT("[Meteor] 玩家不在任何圈内 → 全屏 %.0f 伤"), CachedDamage);
		}
		else if (Outcome == EMeteorOutcome::FailedInRing)
		{
			UChuandiCombatLibrary::ApplyDamage(Boss, Player, CachedDamage);
			UE_LOG(LogChuandi, Log, TEXT("[Meteor] 圈内未弹反 → 落点砸下（距离 %.0fcm，%.0f 伤）"),
				NearestDist, CachedDamage);
		}
	}

	// ── 4. 碎石柱（11-01 用户定：每轮固定 4 根，随机位置，不做"按结果/场景物件"的差异）──
	if (World && Boss && Player && Tuning)
	{
		const int32 PillarTarget = Tuning->RubbleCount;
		const FVector PlayerLoc = Player->GetActorLocation();

		int32 Spawned = 0;
		int32 Guard = 0;
		TArray<FVector> Placed;

		while (Spawned < PillarTarget && Guard++ < 300)
		{
			const float Angle = FMath::FRandRange(0.f, 2.f * PI);
			const float Dist = FMath::FRandRange(Tuning->RubbleMinDistance, Tuning->RubbleMaxDistance);
			const FVector Flat(
				PlayerLoc.X + FMath::Cos(Angle) * Dist,
				PlayerLoc.Y + FMath::Sin(Angle) * Dist,
				0.f);

			// 别贴 Boss、别压在任何人身上（会被夹住）、别和刚生成的块挤成一堵墙
			if (FVector::Dist2D(Flat, Boss->GetActorLocation()) < 400.f
				|| IsPointOccupiedByPawn(World, Flat, 200.f))
			{
				continue;
			}

			bool bTooClose = false;
			for (const FVector& PlacedPoint : Placed)
			{
				if (FVector::Dist2D(PlacedPoint, Flat) < 320.f)
				{
					bTooClose = true;
					break;
				}
			}
			if (bTooClose)
			{
				continue;
			}

			if (SpawnRubbleAt(World, Boss, Flat, Tuning))
			{
				Placed.Add(Flat);
				++Spawned;
			}
		}

		UE_LOG(LogChuandi, Log, TEXT("[Meteor] 落地，生成 %d 根碎石柱（高 %.0f–%.0fcm）"),
			Spawned, Tuning->RubbleHeightMin, Tuning->RubbleHeightMax);
	}

	// ── 5. 收起所有黄圈 ──
	for (ATelegraphActor* Telegraph : Telegraphs)
	{
		if (IsValid(Telegraph))
		{
			Telegraph->Destroy();
		}
	}
	Telegraphs.Reset();

	if (AChuandiBossCharacter* BossChar = Cast<AChuandiBossCharacter>(Boss))
	{
		if (UBossStateMachineComponent* FSM = BossChar->GetStateMachine())
		{
			FSM->NotifyCastFinished();
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
