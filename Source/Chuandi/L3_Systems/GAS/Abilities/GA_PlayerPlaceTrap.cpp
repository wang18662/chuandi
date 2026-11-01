#include "L3_Systems/GAS/Abilities/GA_PlayerPlaceTrap.h"

#include "L0_Core/ChuandiLog.h"
#include "L1_Framework/ChuandiTuningData.h"
#include "L2_Entities/Player/ChuandiPlayerCharacter.h"
#include "L2_Entities/World/TrapActor.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

UGA_PlayerPlaceTrap::UGA_PlayerPlaceTrap()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
}

void UGA_PlayerPlaceTrap::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	AChuandiPlayerCharacter* Player = Cast<AChuandiPlayerCharacter>(GetAvatarActorFromActorInfo());
	UWorld* World = GetWorld();
	if (!Player || !World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	// 07 §2.1：落点由"长按选位"决定，能力本身只负责放置与消耗
	FVector AimPoint;
	if (!Player->ConsumePendingTrapLocation(AimPoint))
	{
		UE_LOG(LogChuandi, Warning, TEXT("[Trap] 没有有效落点，不消耗传递物"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!Player->ConsumeTrapCharge())
	{
		UE_LOG(LogChuandi, Warning, TEXT("[Trap] 传递物不足，无法布阱"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FVector PlayerLocation = Player->GetActorLocation();
	const float PlacedDistance = FVector::Dist2D(AimPoint, PlayerLocation);
	const FVector SpawnLocation = AimPoint + FVector(0.f, 0.f, 3.f);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Player;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	World->SpawnActor<AChuandiTrapActor>(
		AChuandiTrapActor::StaticClass(), SpawnLocation, FRotator::ZeroRotator, SpawnParams);

	UE_LOG(LogChuandi, Log, TEXT("[Trap] 布置成功，距离 %.0fcm，剩余传递物 %d"),
		PlacedDistance, Player->GetTrapCharges());

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
