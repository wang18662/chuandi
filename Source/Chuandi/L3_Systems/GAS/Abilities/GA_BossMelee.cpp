#include "L3_Systems/GAS/Abilities/GA_BossMelee.h"

#include "L0_Core/ChuandiLog.h"
#include "L1_Framework/ChuandiTuningData.h"
#include "L2_Entities/Boss/ChuandiBossCharacter.h"
#include "L2_Entities/World/TelegraphActor.h"
#include "L3_Systems/BossStateMachineComponent.h"
#include "L3_Systems/GAS/ChuandiCombatLibrary.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

UGA_BossMelee::UGA_BossMelee()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
}

void UGA_BossMelee::ActivateAbility(
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

	// 近战动作：有骨架就播攻击动画（时长对齐前摇）
	if (AChuandiBossCharacter* BossChar = Cast<AChuandiBossCharacter>(Boss))
	{
		BossChar->PlayActionAnim(BossChar->GetAttackAnim(), 0.f);
	}

	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float Windup = Tuning ? Tuning->MeleeWindup : 0.4f;
	const float Range = Tuning ? Tuning->MeleeRange : 200.f;

	// 前摇可读性：脚下一个小红圈（没有攻击动画时的替代）
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Telegraph = World->SpawnActor<ATelegraphActor>(
		ATelegraphActor::StaticClass(), Boss->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
	if (Telegraph)
	{
		Telegraph->PlayWarning(Windup, Range, FLinearColor(0.9f, 0.25f, 0.2f, 1.f));
	}

	UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, Windup);
	DelayTask->OnFinish.AddDynamic(this, &UGA_BossMelee::OnMeleeHit);
	DelayTask->ReadyForActivation();
}

void UGA_BossMelee::OnMeleeHit()
{
	UWorld* World = GetWorld();
	AActor* Boss = GetAvatarActorFromActorInfo();
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);

	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float Range = Tuning ? Tuning->MeleeRange : 200.f;
	const float Damage = Tuning ? Tuning->MeleeDamage : 10.f;

	if (World && Boss && Player)
	{
		const float Dist = FVector::Dist2D(Boss->GetActorLocation(), Player->GetActorLocation());
		if (Dist <= Range)
		{
			// 前摇结束时玩家闪开了就打空——这是近战该有的"可躲"
			UChuandiCombatLibrary::ApplyDamage(Boss, Player, Damage);
			UE_LOG(LogChuandi, Log, TEXT("[Melee] Boss 近战命中，伤害 %.0f（距离 %.0fcm）"), Damage, Dist);
		}
		else
		{
			UE_LOG(LogChuandi, Log, TEXT("[Melee] Boss 近战落空（距离 %.0fcm > %.0f）"), Dist, Range);
		}
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

void UGA_BossMelee::EndAbility(
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

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
