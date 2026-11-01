#include "L3_Systems/GAS/Abilities/GA_BossInstaKill.h"

#include "L0_Core/ChuandiLog.h"
#include "L1_Framework/ChuandiTuningData.h"
#include "L2_Entities/Boss/ChuandiBossCharacter.h"
#include "L2_Entities/Player/ChuandiPlayerCharacter.h"
#include "L2_Entities/World/CoverActor.h"
#include "L3_Systems/BossStateMachineComponent.h"
#include "L3_Systems/GAS/ChuandiCombatLibrary.h"

#include "EngineUtils.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

UGA_BossInstaKill::UGA_BossInstaKill()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
}

void UGA_BossInstaKill::ActivateAbility(
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
	if (!World || !Boss)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float CastTime = Tuning ? Tuning->InstaKillCastTime : 10.f;

	if (AChuandiBossCharacter* BossChar = Cast<AChuandiBossCharacter>(Boss))
	{
		// 预警状态交给 Boss 持有：HUD 每帧读它画顶部倒计时，不需要额外事件通道
		BossChar->StartInstaKillWarning(CastTime);
		BossChar->FlashColor(FLinearColor(0.95f, 0.10f, 0.10f, 1.f), CastTime);
	}

	UE_LOG(LogChuandi, Log,
		TEXT("[InstaKill] 开始蓄力 %.1fs —— 玩家必须按 F 站上碎石柱，否则秒杀"), CastTime);

	UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, CastTime);
	DelayTask->OnFinish.AddDynamic(this, &UGA_BossInstaKill::OnResolve);
	DelayTask->ReadyForActivation();
}

void UGA_BossInstaKill::OnResolve()
{
	UWorld* World = GetWorld();
	AActor* Boss = GetAvatarActorFromActorInfo();
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();

	if (AChuandiBossCharacter* BossChar = Cast<AChuandiBossCharacter>(Boss))
	{
		BossChar->ClearInstaKillWarning();
	}

	if (World && Boss && Player)
	{
		AChuandiPlayerCharacter* PlayerChar = Cast<AChuandiPlayerCharacter>(Player);
		AChuandiCoverActor* StandingPillar = PlayerChar ? PlayerChar->GetStandingPillar() : nullptr;

		// 先收集全场柱子：这一击是"全屏冲击"，会把所有柱子一起推平（10-30 用户要求）
		TArray<AChuandiCoverActor*> AllPillars;
		for (TActorIterator<AChuandiCoverActor> It(World); It; ++It)
		{
			AllPillars.Add(*It);
		}

		if (StandingPillar)
		{
			// 柱子替玩家挡下这一击（它自己也会在下面被推平），玩家落在原地
			UChuandiCombatLibrary::HitStop(World, 0.12f, 0.1f);
			UE_LOG(LogChuandi, Log, TEXT("[InstaKill] 玩家站在柱子上 → 躲过"));
		}
		else
		{
			const float Damage = Tuning ? Tuning->InstaKillDamage : 9999.f;
			UChuandiCombatLibrary::ApplyDamage(Boss, Player, Damage);
			UE_LOG(LogChuandi, Log, TEXT("[InstaKill] 玩家不在柱子上 → 秒杀（%.0f 伤）"), Damage);
		}

		// 全屏冲击：所有柱子一起消失（不只是脚下那根）
		int32 Destroyed = 0;
		for (AChuandiCoverActor* Pillar : AllPillars)
		{
			if (IsValid(Pillar))
			{
				Pillar->ConsumePillar(TEXT("被全屏秒杀摧毁"));
				++Destroyed;
			}
		}
		UE_LOG(LogChuandi, Log, TEXT("[InstaKill] 全屏冲击摧毁 %d 根柱子"), Destroyed);
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

void UGA_BossInstaKill::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// 被破防/冰冻打断：预警必须一起撤掉，否则 HUD 会一直显示倒计时
	if (bWasCancelled)
	{
		if (AChuandiBossCharacter* BossChar = Cast<AChuandiBossCharacter>(GetAvatarActorFromActorInfo()))
		{
			BossChar->ClearInstaKillWarning();
			if (UBossStateMachineComponent* FSM = BossChar->GetStateMachine())
			{
				FSM->NotifyCastFinished();
			}
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
