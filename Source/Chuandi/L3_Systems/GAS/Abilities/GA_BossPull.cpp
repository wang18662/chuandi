#include "L3_Systems/GAS/Abilities/GA_BossPull.h"

#include "L0_Core/ChuandiLog.h"
#include "L1_Framework/ChuandiTuningData.h"
#include "L2_Entities/Boss/ChuandiBossCharacter.h"
#include "L2_Entities/Player/ChuandiPlayerCharacter.h"
#include "L2_Entities/World/TelegraphActor.h"
#include "L3_Systems/BossStateMachineComponent.h"
#include "L3_Systems/BreakMeterComponent.h"
#include "L3_Systems/GAS/ChuandiAbilitySystemComponent.h"
#include "L3_Systems/GAS/ChuandiCombatLibrary.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UGA_BossPull::UGA_BossPull()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
}

void UGA_BossPull::ActivateAbility(
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
	const float Windup = Tuning ? Tuning->PullWindup : 0.8f;

	if (AChuandiBossCharacter* BossChar = Cast<AChuandiBossCharacter>(Boss))
	{
		BossChar->FaceActor(Player);
		// 抬手：闪一下紫，和"陨石黄 / 近战红 / 遁地暗红"区分开
		BossChar->FlashColor(FLinearColor(0.65f, 0.30f, 0.95f, 1.f), Windup);
	}

	// 锁定圈画在**玩家脚下**（不是 Boss 脚下）：这一招的信息是"你被锁定了"，
	// 画在 Boss 身上玩家会以为是 AOE，反而往 Boss 方向跑。
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Telegraph = World->SpawnActor<ATelegraphActor>(
		ATelegraphActor::StaticClass(), Player->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
	if (Telegraph)
	{
		Telegraph->PlayWarning(Windup, 150.f, FLinearColor(0.65f, 0.30f, 0.95f, 1.f));
	}

	UE_LOG(LogChuandi, Log, TEXT("[Pull] 锁定开始，抬手 %.2fs"), Windup);

	UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, Windup);
	DelayTask->OnFinish.AddDynamic(this, &UGA_BossPull::OnLockFinished);
	DelayTask->ReadyForActivation();
}

void UGA_BossPull::OnLockFinished()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
		return;
	}

	if (IsValid(Telegraph))
	{
		Telegraph->Destroy();
	}
	Telegraph = nullptr;

	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float Speed = Tuning ? Tuning->PullSpeed : 700.f;
	UE_LOG(LogChuandi, Log, TEXT("[Pull] 拉拽开始（%.0fcm/s，最长 %.1fs）"),
		Speed, Tuning ? Tuning->PullDuration : 1.2f);

	PullElapsed = 0.f;
	bSweepStarted = false;
	World->GetTimerManager().SetTimer(PullTimer, this, &UGA_BossPull::TickPull, 0.02f, true);
}

void UGA_BossPull::TickPull()
{
	UWorld* World = GetWorld();
	AActor* Boss = GetAvatarActorFromActorInfo();
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);

	if (!World || !Boss || !Player)
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(PullTimer);
		}
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
		return;
	}

	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float Speed = Tuning ? Tuning->PullSpeed : 700.f;
	const float StopDist = Tuning ? Tuning->PullStopDistance : 300.f;
	const float MaxDuration = Tuning ? Tuning->PullDuration : 1.2f;

	const float DeltaSeconds = World->GetDeltaSeconds();
	PullElapsed += DeltaSeconds;

	FVector ToBoss = Boss->GetActorLocation() - Player->GetActorLocation();
	ToBoss.Z = 0.f;
	const float Dist = ToBoss.Size();

	if (Dist <= StopDist || PullElapsed >= MaxDuration)
	{
		World->GetTimerManager().ClearTimer(PullTimer);
		StartSweep();
		return;
	}

	// 拉拽用 sweep 移动（撞到墙/碎石会停下，不会把玩家塞进几何体）
	// 注意：玩家自己的输入仍然有效（4m/s 反向跑只能抵消一部分，净速度 3m/s 靠近）
	// —— 这不是漏做"定身"，而是设计：跑不掉的感觉来自"拉得比你快"。
	const FVector Step = ToBoss.GetSafeNormal() * (Speed * DeltaSeconds);
	Player->AddActorWorldOffset(Step, true);
}

void UGA_BossPull::StartSweep()
{
	if (bSweepStarted)
	{
		return;
	}
	bSweepStarted = true;

	UWorld* World = GetWorld();
	AActor* Boss = GetAvatarActorFromActorInfo();
	if (!World || !Boss)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
		return;
	}

	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float SweepWindup = Tuning ? Tuning->PullSweepWindup : 0.5f;
	const float SweepRadius = Tuning ? Tuning->PullSweepRadius : 350.f;

	if (AChuandiBossCharacter* BossChar = Cast<AChuandiBossCharacter>(Boss))
	{
		BossChar->FaceActor(UGameplayStatics::GetPlayerPawn(this, 0));
		BossChar->PlayActionAnim(BossChar->GetAttackAnim(), SweepWindup);
	}

	// 禁锢（10-29 用户要求）：拉到位就把玩家钉在地上，接下来 5s 只能站着弹反。
	// 注意用的是独立标记（BeginForceRoot），否则玩家在这 5s 里按一次 Q，
	// 弹反窗口一结束就会把禁锢一起解掉（EndParryRoot 会恢复移动）。
	if (AChuandiPlayerCharacter* PlayerChar = Cast<AChuandiPlayerCharacter>(
		UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		PlayerChar->BeginForceRoot();
	}

	// 红圈 = "这一下必须弹"，5s 前摇本身就是倒计时（圈里的填充会慢慢涨满）
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Telegraph = World->SpawnActor<ATelegraphActor>(
		ATelegraphActor::StaticClass(), Boss->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
	if (Telegraph)
	{
		Telegraph->PlayWarning(SweepWindup, SweepRadius, FLinearColor(1.f, 0.25f, 0.15f, 1.f));
	}

	UE_LOG(LogChuandi, Log, TEXT("[Pull] 横扫前摇 %.2fs（按 Q 弹反，成功破防 +%.0f）"),
		SweepWindup, Tuning ? Tuning->PullGaugeOnParry : 25.f);

	UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, SweepWindup);
	DelayTask->OnFinish.AddDynamic(this, &UGA_BossPull::OnSweepFinished);
	DelayTask->ReadyForActivation();
}

void UGA_BossPull::OnSweepFinished()
{
	UWorld* World = GetWorld();
	AActor* Boss = GetAvatarActorFromActorInfo();
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();

	if (IsValid(Telegraph))
	{
		Telegraph->Destroy();
	}
	Telegraph = nullptr;

	const float SweepRadius = Tuning ? Tuning->PullSweepRadius : 350.f;
	const float Damage = Tuning ? Tuning->PullSweepDamage : 20.f;
	const float Gauge = Tuning ? Tuning->PullGaugeOnParry : 25.f;

	if (World && Boss && Player)
	{
		const float Dist = FVector::Dist2D(Boss->GetActorLocation(), Player->GetActorLocation());

		// 玩家在 0.5s 前摇里跑出横扫范围 → 落空（这是"闪避/跑位"的解法，不是失败态）
		if (Dist > SweepRadius)
		{
			UE_LOG(LogChuandi, Log, TEXT("[Pull] 横扫落空：玩家跑出范围（%.0f > %.0f）"), Dist, SweepRadius);
		}
		else
		{
			bool bParried = false;
			if (UChuandiAbilitySystemComponent* PlayerASC = Cast<UChuandiAbilitySystemComponent>(
				UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player)))
			{
				if (PlayerASC->IsParryWindowOpen(World->GetTimeSeconds()))
				{
					PlayerASC->ConsumeParryWindow();
					bParried = true;
				}
			}

			if (bParried)
			{
				if (AChuandiBossCharacter* BossChar = Cast<AChuandiBossCharacter>(Boss))
				{
					BossChar->FlashColor(FLinearColor(1.f, 0.83f, 0.28f, 1.f), 0.2f);
					if (UBreakMeterComponent* Meter = BossChar->GetBreakMeter())
					{
						Meter->AddGauge(Gauge);
					}
				}
				UChuandiCombatLibrary::HitStop(World, 0.1f, 0.05f);
				UE_LOG(LogChuandi, Log, TEXT("[Pull] 横扫被弹反，破防 +%.0f"), Gauge);
			}
			else
			{
				UChuandiCombatLibrary::ApplyDamage(Boss, Player, Damage);
				UE_LOG(LogChuandi, Log, TEXT("[Pull] 横扫命中，伤害 %.0f（距离 %.0f）"), Damage, Dist);
			}
		}
	}

	// 不管弹没弹到，禁锢都在这一帧解除
	if (AChuandiPlayerCharacter* PlayerChar = Cast<AChuandiPlayerCharacter>(Player))
	{
		PlayerChar->EndForceRoot();
	}

	if (AChuandiBossCharacter* BossChar = Cast<AChuandiBossCharacter>(Boss))
	{
		if (UBossStateMachineComponent* FSM = BossChar->GetStateMachine())
		{
			FSM->NotifyCastFinished();
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_BossPull::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PullTimer);
	}

	if (IsValid(Telegraph))
	{
		Telegraph->Destroy();
	}
	Telegraph = nullptr;

	// 兜底：技能被破防/冰冻打断时，玩家不能被永久定住
	if (AChuandiPlayerCharacter* PlayerChar = Cast<AChuandiPlayerCharacter>(
		UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		PlayerChar->EndForceRoot();
	}

	// 被破防/冰冻打断时，状态机的 bCasting 还挂着 true —— 必须在这里放掉，
	// 否则 Boss 会永久停在 Casting（遁地的 EndAbility 也是同一个理由）。
	if (AChuandiBossCharacter* BossChar = Cast<AChuandiBossCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UBossStateMachineComponent* FSM = BossChar->GetStateMachine())
		{
			if (bWasCancelled)
			{
				FSM->NotifyCastFinished();
			}
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
