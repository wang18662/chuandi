#include "L3_Systems/BossPerceptionComponent.h"

#include "L0_Core/ChuandiLog.h"
#include "L1_Framework/ChuandiTuningData.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

UBossPerceptionComponent::UBossPerceptionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// 20Hz 足够：感知不需要每帧
	PrimaryComponentTick.TickInterval = 0.05f;
}

bool UBossPerceptionComponent::IsTargetForgotten(double Now) const
{
	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float Memory = Tuning ? Tuning->AwarenessMemory : 4.f;
	return (Now - LastSeenTime) > Memory;
}

void UBossPerceptionComponent::ForceAwareness(const FVector& Location)
{
	LastKnownLocation = Location;
	Suspicion = 1.f;

	if (const UWorld* World = GetWorld())
	{
		LastSeenTime = World->GetTimeSeconds();
	}
}

void UBossPerceptionComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdatePerception(DeltaTime);
}

void UBossPerceptionComponent::UpdatePerception(float DeltaTime)
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Owner || !World || !Player)
	{
		return;
	}

	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float SightRadius = Tuning ? Tuning->PerceptionRadius : 800.f;
	const float HalfAngleCos = FMath::Cos(FMath::DegreesToRadians(
		Tuning ? Tuning->PerceptionHalfAngle : 60.f));
	const float ConfirmTime = FMath::Max(0.01f, Tuning ? Tuning->SuspicionConfirmTime : 0.3f);

	const FVector EyeLocation = Owner->GetActorLocation() + FVector(0.f, 0.f, 100.f);
	const FVector PlayerChest = Player->GetActorLocation();
	const FVector ToPlayer = PlayerChest - EyeLocation;
	const float Distance = ToPlayer.Size();

	// ① 距离
	bInCone = false;
	if (Distance <= SightRadius && Distance > KINDA_SMALL_NUMBER)
	{
		// ② 前向夹角
		const float Dot = FVector::DotProduct(Owner->GetActorForwardVector(), ToPlayer / Distance);
		bInCone = Dot >= HalfAngleCos;
	}

	// ③ 遮挡：打到玩家自己算通视，打到别的东西算被挡
	bool bClear = false;
	if (bInCone)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BossSight), false, Owner);
		Params.AddIgnoredActor(Owner);

		const bool bHit = World->LineTraceSingleByChannel(
			Hit, EyeLocation, PlayerChest, ECC_Visibility, Params);
		bClear = !bHit || Hit.GetActor() == Player;
	}

	bHasLineOfSight = bInCone && bClear;

	// 诊断：在视野半径内却看不见时，说清是哪一条没过
	if (!bHasLineOfSight && Distance <= SightRadius)
	{
		DiagnoseAccum += DeltaTime;
		if (DiagnoseAccum >= 2.f)
		{
			DiagnoseAccum = 0.f;
			UE_LOG(LogChuandi, Log,
				TEXT("[Percept] 玩家在 %.0fcm 内但看不见：角度通过=%d 遮挡通过=%d（Suspicion=%.2f）"),
				Distance, bInCone ? 1 : 0, bClear ? 1 : 0, Suspicion);
		}
	}
	else
	{
		DiagnoseAccum = 0.f;
	}

	const double Now = World->GetTimeSeconds();
	if (bHasLineOfSight)
	{
		LastKnownLocation = Player->GetActorLocation();
		LastSeenTime = Now;
		Suspicion = FMath::Min(1.f, Suspicion + DeltaTime / ConfirmTime);
	}
	else
	{
		// 看不见就慢慢失去怀疑（1 秒掉完）
		Suspicion = FMath::Max(0.f, Suspicion - DeltaTime / 1.f);
	}
}
