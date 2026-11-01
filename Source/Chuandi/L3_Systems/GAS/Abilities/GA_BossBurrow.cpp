#include "L3_Systems/GAS/Abilities/GA_BossBurrow.h"

#include "L0_Core/ChuandiLog.h"
#include "L0_Core/ChuandiNativeTags.h"
#include "L1_Framework/ChuandiTuningData.h"
#include "L2_Entities/Boss/ChuandiBossCharacter.h"
#include "L2_Entities/World/TelegraphActor.h"
#include "L3_Systems/BossStateMachineComponent.h"
#include "L3_Systems/BreakMeterComponent.h"
#include "L3_Systems/GAS/ChuandiAbilitySystemComponent.h"
#include "L3_Systems/GAS/ChuandiCombatLibrary.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

UGA_BossBurrow::UGA_BossBurrow()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
}

void UGA_BossBurrow::ActivateAbility(
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
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!World || !Boss || !ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float SinkTime = Tuning ? Tuning->BurrowSinkTime : 0.8f;
	CachedRadius = Tuning ? Tuning->BurrowRadius : 400.f;
	CachedDamage = Tuning ? Tuning->BurrowDamage : 40.f;

	// 遁地：不可选中 + 免疫冰冻（R1）
	ASC->AddLooseGameplayTag(ChuandiTags::State_Burrowed.GetTag());

	// 地面裂纹（可读性）：先给一个小的下沉预警圈
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Telegraph = World->SpawnActor<ATelegraphActor>(
		ATelegraphActor::StaticClass(), Boss->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
	if (Telegraph)
	{
		Telegraph->PlayWarning(SinkTime, CachedRadius * 0.4f, FLinearColor(0.75f, 0.15f, 0.12f, 1.f));
	}

	UE_LOG(LogChuandi, Log, TEXT("[Burrow] 开始遁地（下沉 %.2fs）"), SinkTime);

	UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, SinkTime);
	DelayTask->OnFinish.AddDynamic(this, &UGA_BossBurrow::OnSinkFinished);
	DelayTask->ReadyForActivation();
}

void UGA_BossBurrow::OnSinkFinished()
{
	UWorld* World = GetWorld();
	AActor* Boss = GetAvatarActorFromActorInfo();
	if (!World || !Boss)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
		return;
	}

	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float Windup = Tuning ? Tuning->BurrowWindup : 1.5f;

	// 出土前摇：大圈预告 1.5s（07 §3.3）
	if (IsValid(Telegraph))
	{
		Telegraph->Destroy();
	}
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Telegraph = World->SpawnActor<ATelegraphActor>(
		ATelegraphActor::StaticClass(), Boss->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
	if (Telegraph)
	{
		Telegraph->PlayWarning(Windup, CachedRadius, FLinearColor(0.85f, 0.35f, 0.15f, 1.f));
	}

	UE_LOG(LogChuandi, Log, TEXT("[Burrow] 出土前摇 %.2fs，半径 %.0f"), Windup, CachedRadius);

	UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, Windup);
	DelayTask->OnFinish.AddDynamic(this, &UGA_BossBurrow::OnStrike);
	DelayTask->ReadyForActivation();
}

void UGA_BossBurrow::OnStrike()
{
	UWorld* World = GetWorld();
	AActor* Boss = GetAvatarActorFromActorInfo();
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);

	if (World && Boss && Player)
	{
		// 07 §3.3（11:33 版）：遁地**不可弹反**，唯一解法是跑出 4m。
		// 原因：弹反源只保留陨石（+35），否则破防节奏会翻倍。
		const float Dist = FVector::Dist2D(Boss->GetActorLocation(), Player->GetActorLocation());
		if (Dist <= CachedRadius)
		{
			UChuandiCombatLibrary::ApplyDamage(Boss, Player, CachedDamage);
			UE_LOG(LogChuandi, Log, TEXT("[Burrow] 出土命中，伤害 %.0f（距离 %.0fcm ≤ %.0f，不可弹反）"),
				CachedDamage, Dist, CachedRadius);
		}
		else
		{
			UE_LOG(LogChuandi, Log, TEXT("[Burrow] 出土落空：玩家跑出范围（%.0f > %.0f）"), Dist, CachedRadius);
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_BossBurrow::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (IsValid(Telegraph))
	{
		Telegraph->Destroy();
	}
	Telegraph = nullptr;

	// 出土：解除"不可选中 / 免疫冰冻"
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveLooseGameplayTag(ChuandiTags::State_Burrowed.GetTag());
	}

	if (AChuandiBossCharacter* BossChar = Cast<AChuandiBossCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UBossStateMachineComponent* FSM = BossChar->GetStateMachine())
		{
			FSM->NotifyCastFinished();
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
