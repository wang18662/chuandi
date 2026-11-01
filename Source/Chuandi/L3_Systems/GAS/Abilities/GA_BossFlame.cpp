#include "L3_Systems/GAS/Abilities/GA_BossFlame.h"

#include "L0_Core/ChuandiLog.h"
#include "L1_Framework/ChuandiTuningData.h"
#include "L2_Entities/Boss/ChuandiBossCharacter.h"
#include "L2_Entities/World/ChuandiFlameZoneActor.h"
#include "L3_Systems/BossStateMachineComponent.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

UGA_BossFlame::UGA_BossFlame()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
}

void UGA_BossFlame::ActivateAbility(
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
	const float Length = Tuning ? Tuning->FlameLength : 1400.f;
	const float Width = Tuning ? Tuning->FlameWidth : 240.f;
	const float StartOffset = Tuning ? Tuning->FlameStartOffset : 100.f;
	const float Warning = Tuning ? Tuning->FlameWarningTime : 1.0f;
	const float Duration = Tuning ? Tuning->FlameDuration : 4.f;
	const float Interval = Tuning ? Tuning->FlameTickInterval : 0.5f;
	const float Damage = Tuning ? Tuning->FlameTickDamage : 3.f;

	// 先转身：长条的方向 = Boss 朝向，转身之后再铺，条才会穿过玩家
	if (AChuandiBossCharacter* BossChar = Cast<AChuandiBossCharacter>(Boss))
	{
		BossChar->FaceActor(Player);
	}

	const FRotator Yaw = FRotator(0.f, Boss->GetActorRotation().Yaw, 0.f);
	const FVector Forward = Yaw.Vector();

	// 条的中心 = Boss 身前 StartOffset 处再向前半条长（这样起点贴着 Boss、终点 = Offset + Length）
	FVector Center = Boss->GetActorLocation() + Forward * (StartOffset + Length * 0.5f);

	// 贴地：从中心上方往下打一条射线，避免长方块悬空或被地面吃掉
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(FlameGround), false, Boss);
		if (World->LineTraceSingleByChannel(Hit, Center + FVector(0.f, 0.f, 400.f),
			Center - FVector(0.f, 0.f, 600.f), ECC_Visibility, Params))
		{
			Center.Z = Hit.ImpactPoint.Z + 2.f;
		}
		else
		{
			Center.Z = Boss->GetActorLocation().Z - 88.f + 2.f;
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Zone = World->SpawnActor<AChuandiFlameZoneActor>(
		AChuandiFlameZoneActor::StaticClass(), Center, Yaw, SpawnParams);
	if (Zone)
	{
		Zone->BeginStrip(Length, Width, Warning, Duration, Interval, Damage, Boss);
	}

	UE_LOG(LogChuandi, Log,
		TEXT("[Flame] 沿朝向铺火 %.0f×%.0f（预警 %.1fs，燃烧 %.0fs，每 %.1fs %.0f 伤）"),
		Length, Width, Warning, Duration, Interval, Damage);

	UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, Warning);
	DelayTask->OnFinish.AddDynamic(this, &UGA_BossFlame::OnFlameLanded);
	DelayTask->ReadyForActivation();
}

void UGA_BossFlame::OnFlameLanded()
{
	// 火已经在烧了（Zone 自己管生命周期），这里只负责把 Boss 放回追击
	if (AChuandiBossCharacter* BossChar = Cast<AChuandiBossCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UBossStateMachineComponent* FSM = BossChar->GetStateMachine())
		{
			FSM->NotifyCastFinished();
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_BossFlame::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// 只有被中断（破防/冰冻）才把还没烧起来的火撤掉；
	// 正常结束时火要继续烧，所以不能无条件 Destroy。
	if (bWasCancelled)
	{
		if (IsValid(Zone))
		{
			Zone->Destroy();
		}
		Zone = nullptr;

		if (AChuandiBossCharacter* BossChar = Cast<AChuandiBossCharacter>(GetAvatarActorFromActorInfo()))
		{
			if (UBossStateMachineComponent* FSM = BossChar->GetStateMachine())
			{
				FSM->NotifyCastFinished();
			}
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
