#include "L3_Systems/GAS/Abilities/GA_BossCharge.h"

#include "L0_Core/ChuandiLog.h"
#include "L1_Framework/ChuandiTuningData.h"
#include "L2_Entities/Boss/ChuandiBossCharacter.h"
#include "L2_Entities/Player/ChuandiPlayerCharacter.h"
#include "L2_Entities/World/CoverActor.h"
#include "L2_Entities/World/TelegraphActor.h"
#include "L3_Systems/BossStateMachineComponent.h"
#include "L3_Systems/BreakMeterComponent.h"
#include "L3_Systems/GAS/ChuandiCombatLibrary.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UGA_BossCharge::UGA_BossCharge()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
}

void UGA_BossCharge::ActivateAbility(
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
		if (AChuandiBossCharacter* BossChar = Cast<AChuandiBossCharacter>(Boss))
		{
			if (UBossStateMachineComponent* FSM = BossChar->GetStateMachine())
			{
				FSM->NotifyCastFinished();
			}
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float Windup = Tuning ? Tuning->ChargeWindup : 5.f;

	if (AChuandiBossCharacter* BossChar = Cast<AChuandiBossCharacter>(Boss))
	{
		BossChar->FaceActor(Player);
		BossChar->PlayActionAnim(BossChar->GetAttackAnim(), Windup);
		BossChar->StartChargeLock(Windup);
		BossChar->FlashColor(FLinearColor(0.95f, 0.45f, 0.10f, 1.f), 0.4f);
	}

	// 锁定圈画在**玩家脚下**：这一招锁的是人，不是柱子
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Telegraph = World->SpawnActor<ATelegraphActor>(
		ATelegraphActor::StaticClass(), Player->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
	if (Telegraph)
	{
		Telegraph->PlayWarning(Windup, 220.f, FLinearColor(0.95f, 0.25f, 0.10f, 1.f));
	}

	UE_LOG(LogChuandi, Log,
		TEXT("[Charge] 锁定玩家 %.1fs —— 玩家需把碎石柱摆到自己和 Boss 之间"), Windup);

	UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, Windup);
	DelayTask->OnFinish.AddDynamic(this, &UGA_BossCharge::OnWindupFinished);
	DelayTask->ReadyForActivation();
}

void UGA_BossCharge::OnWindupFinished()
{
	UWorld* World = GetWorld();
	AActor* Boss = GetAvatarActorFromActorInfo();
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!World || !Boss || !Player)
	{
		FinishCharge(TEXT("Act 失效"));
		return;
	}

	if (IsValid(Telegraph))
	{
		Telegraph->Destroy();
	}
	Telegraph = nullptr;

	if (AChuandiBossCharacter* BossChar = Cast<AChuandiBossCharacter>(Boss))
	{
		BossChar->ClearChargeLock();
		BossChar->FaceActor(Player);
	}

	// ★ 方向在**起飞这一刻**才取：玩家必须在 5s 内把柱子安排在自己与 Boss 的连线上，
	//   而不是站在原地"等它冲过来再侧闪"。
	ChargeDirection = (Player->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D();
	if (ChargeDirection.IsNearlyZero())
	{
		ChargeDirection = Boss->GetActorForwardVector();
	}

	ChargeElapsed = 0.f;
	bFinished = false;
	PillarsBrokenInThisCharge = 0;

	UE_LOG(LogChuandi, Log, TEXT("[Charge] 起飞（13m/s，直线朝玩家）"));
	World->GetTimerManager().SetTimer(ChargeTimer, this, &UGA_BossCharge::TickCharge, 0.016f, true);
}

bool UGA_BossCharge::BreakPillar(AChuandiCoverActor* Pillar)
{
	if (!IsValid(Pillar))
	{
		return false;
	}

	AActor* Boss = GetAvatarActorFromActorInfo();
	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();

	Pillar->ConsumePillar(TEXT("被 Boss 冲撞撞碎"));

	AChuandiBossCharacter* BossChar = Cast<AChuandiBossCharacter>(Boss);
	if (!BossChar)
	{
		return false;
	}

	// 07 §3.3-D：每撞碎 1 根 → 破防条 +15（保底，自动）
	const float PerPillar = Tuning ? Tuning->ChargeGaugePerPillar : 15.f;
	if (UBreakMeterComponent* Meter = BossChar->GetBreakMeter())
	{
		Meter->AddGauge(PerPillar);
	}

	++PillarsBrokenInThisCharge;
	const int32 Needed = Tuning ? Tuning->ChargeBreakCount : 3;
	UE_LOG(LogChuandi, Log, TEXT("[Charge] 撞毁柱子 %d/%d（破防 +%.0f）"),
		PillarsBrokenInThisCharge, Needed, PerPillar);

	if (PillarsBrokenInThisCharge >= Needed)
	{
		// 07 §3.3-D（11-01 定稿）：撞满 3 根 = 34×3 = 102 → 破防条封顶，**立即破防**，不需要按键
		const float Window = Tuning ? Tuning->ChargeBreakStagger : 0.8f;
		BossChar->ApplyStagger(Window);
		BossChar->FlashColor(FLinearColor(1.f, 0.85f, 0.35f, 1.f), Window);
		UE_LOG(LogChuandi, Log, TEXT("[Charge] 撞满 %d 根 → 立即破防 + 僵直 %.1fs"), Needed, Window);
		return true;
	}

	// 没撞满：继续往前撞（连续冲撞），给一个短停顿做节奏
	BossChar->ApplyStagger(Tuning ? Tuning->BossCoverBreakStagger : 0.5f);
	return false;
}

void UGA_BossCharge::TickCharge()
{
	if (bFinished)
	{
		return;
	}

	UWorld* World = GetWorld();
	AActor* Boss = GetAvatarActorFromActorInfo();
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!World || !Boss)
	{
		FinishCharge(TEXT("Act 失效"));
		return;
	}

	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float Speed = Tuning ? Tuning->ChargeSpeed : 1300.f;
	const float HitRadius = Tuning ? Tuning->ChargeHitRadius : 220.f;
	const float MaxDuration = Tuning ? Tuning->ChargeMaxDuration : 3.f;
	const float PlayerDamage = Tuning ? Tuning->ChargePlayerDamage : 15.f;

	const float DeltaSeconds = World->GetDeltaSeconds();
	ChargeElapsed += DeltaSeconds;

	// 追踪：每拍重算"朝玩家"的方向（跑是跑不掉的，13 m/s vs 4 m/s）
	if (Player)
	{
		const FVector ToPlayer = (Player->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D();
		if (!ToPlayer.IsNearlyZero())
		{
			ChargeDirection = ToPlayer;
		}
	}

	const FVector NewLocation = Boss->GetActorLocation() + ChargeDirection * Speed * DeltaSeconds;

	FHitResult Hit;
	const bool bMoved = Boss->SetActorLocation(NewLocation, /*bSweep=*/true, &Hit);

	// ── ① 柱子优先（11-01 定）：只要柱子能挡下，就由柱子承担，玩家不掉血 ──
	//    玩家站在柱子上 / 躲在柱子后面，都会走这一支。
	AChuandiCoverActor* HitCover = bMoved ? nullptr : Cast<AChuandiCoverActor>(Hit.GetActor());
	if (!HitCover)
	{
		// 扫掠贴着柱子边缘滑过时，按距离再判一次（柱体宽 1.8–2.4m，容易擦边）
		for (TActorIterator<AChuandiCoverActor> It(World); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), Boss->GetActorLocation()) <= HitRadius)
			{
				HitCover = *It;
				break;
			}
		}
	}

	if (HitCover)
	{
		BreakPillar(HitCover);
		FinishCharge(TEXT("撞碎柱子"));
		return;
	}

	// ── ② 没有柱子挡在身前：撞到玩家就停 + 掉血 ──
	if (Player && FVector::Dist2D(Boss->GetActorLocation(), Player->GetActorLocation()) <= 200.f)
	{
		// 玩家**站在柱子顶上**时，柱子挡在中间：这一下由那根柱子承担，玩家不掉血
		if (const AChuandiPlayerCharacter* PlayerChar = Cast<AChuandiPlayerCharacter>(Player))
		{
			if (AChuandiCoverActor* Under = PlayerChar->GetStandingPillar())
			{
				BreakPillar(Under);
				FinishCharge(TEXT("撞碎玩家脚下的柱子"));
				return;
			}
		}

		UChuandiCombatLibrary::ApplyDamage(Boss, Player, PlayerDamage);
		UE_LOG(LogChuandi, Log, TEXT("[Charge] 撞到玩家，%.0f 伤（周围没有柱子挡下）"), PlayerDamage);
		FinishCharge(TEXT("撞到玩家"));
		return;
	}

	// ── ③ 撞到其它障碍 / 超时 ──
	if (!bMoved && Hit.GetActor())
	{
		FinishCharge(TEXT("撞到其它障碍"));
		return;
	}

	if (ChargeElapsed >= MaxDuration)
	{
		FinishCharge(TEXT("超时"));
	}
}

void UGA_BossCharge::FinishCharge(const TCHAR* Reason)
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeTimer);
	}

	if (IsValid(Telegraph))
	{
		Telegraph->Destroy();
	}
	Telegraph = nullptr;

	UE_LOG(LogChuandi, Log, TEXT("[Charge] 结束：%s"), Reason);

	if (AChuandiBossCharacter* BossChar = Cast<AChuandiBossCharacter>(GetAvatarActorFromActorInfo()))
	{
		BossChar->ClearChargeLock();
		if (UBossStateMachineComponent* FSM = BossChar->GetStateMachine())
		{
			FSM->NotifyCastFinished();
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_BossCharge::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeTimer);
	}

	if (IsValid(Telegraph))
	{
		Telegraph->Destroy();
	}
	Telegraph = nullptr;

	// 被打断（破防/冰冻）：锁定提示必须撤掉，FSM 的 bCasting 也要放掉
	if (bWasCancelled)
	{
		if (AChuandiBossCharacter* BossChar = Cast<AChuandiBossCharacter>(GetAvatarActorFromActorInfo()))
		{
			BossChar->ClearChargeLock();
			if (UBossStateMachineComponent* FSM = BossChar->GetStateMachine())
			{
				FSM->NotifyCastFinished();
			}
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
