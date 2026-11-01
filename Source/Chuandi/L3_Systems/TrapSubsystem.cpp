#include "L3_Systems/TrapSubsystem.h"

#include "L0_Core/ChuandiLog.h"
#include "L0_Core/ChuandiNativeTags.h"
#include "L1_Framework/ChuandiTuningData.h"
#include "L2_Entities/Boss/ChuandiBossCharacter.h"
#include "L2_Entities/World/TrapActor.h"
#include "L3_Systems/BreakMeterComponent.h"
#include "L3_Systems/GAS/ChuandiCombatLibrary.h"
#include "L3_Systems/GAS/ChuandiFrozenEffect.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemInterface.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr float ScanIntervalSeconds = 1.f / 30.f;
}

void UTrapSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Traps.Reset();
	TimeSinceScan = 0.f;
}

bool UTrapSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// 只在真正运行的世界里跑，编辑器预览世界不要
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UTrapSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTrapSubsystem, STATGROUP_Tickables);
}

void UTrapSubsystem::RegisterTrap(AChuandiTrapActor* Trap)
{
	if (Trap)
	{
		Traps.AddUnique(Trap);
	}
}

void UTrapSubsystem::UnregisterTrap(AChuandiTrapActor* Trap)
{
	Traps.Remove(Trap);
}

void UTrapSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TimeSinceScan += DeltaTime;
	if (TimeSinceScan < ScanIntervalSeconds)
	{
		return;
	}
	TimeSinceScan = 0.f;

	CheckTraps();
}

void UTrapSubsystem::CheckTraps()
{
	UWorld* World = GetWorld();
	if (!World || Traps.Num() == 0)
	{
		return;
	}

	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float Lifetime = Tuning ? Tuning->TrapLifetime : 10.f;

	const double Now = World->GetTimeSeconds();
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);

	// 候选人每帧重取：这个 Demo 里场上只有几个 actor，成本可忽略
	TArray<AActor*> Candidates;
	UGameplayStatics::GetAllActorsWithInterface(World, UAbilitySystemInterface::StaticClass(), Candidates);

	for (int32 Index = Traps.Num() - 1; Index >= 0; --Index)
	{
		AChuandiTrapActor* Trap = Traps[Index];
		if (!IsValid(Trap))
		{
			Traps.RemoveAt(Index);
			continue;
		}

		// ① 寿命（D28：10s 未触发即消失）
		if (Now - Trap->GetSpawnTime() >= Lifetime)
		{
			UE_LOG(LogChuandi, Log, TEXT("[Trap] 超时消失（%.1fs 未触发）"), Lifetime);
			Trap->Destroy();
			continue;
		}

		// ② 触发：找一个踩进来的、有 ASC 的、不是玩家的目标
		for (AActor* Candidate : Candidates)
		{
			if (!Candidate || Candidate == Trap || Candidate == PlayerPawn)
			{
				continue;
			}

			const float Dist = FVector::Dist2D(Candidate->GetActorLocation(), Trap->GetActorLocation());
			if (Dist > Trap->GetRadius())
			{
				continue;
			}

			// 遁地免疫冰冻（R1）
			if (UChuandiCombatLibrary::HasTag(Candidate, ChuandiTags::State_Burrowed.GetTag()))
			{
				UE_LOG(LogChuandi, Log, TEXT("[Trap] %s 正在遁地，免疫冰冻"), *GetNameSafe(Candidate));
				Trap->Destroy();
				break;
			}

			// 冰冻（时长从 TuningData 来，只控不破）
			const float FreezeDuration = Tuning ? Tuning->TrapFreezeDuration : 3.f;
			UChuandiCombatLibrary::ApplyStatusGEWithTags(Trap, Candidate,
				UChuandiFrozenEffect::StaticClass(), ChuandiTags::State_Frozen.GetTag(),
				1.f, FreezeDuration);

			// 冰冻要能打断正在施法的技能，否则"控制"只是日志上的一行
			if (UAbilitySystemComponent* TargetASC =
				UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Candidate))
			{
				TargetASC->CancelAllAbilities();
			}

			// 07 §1.8：陷阱触发 +15 破防条（控制转进度，让"最贵的传递手法"不亏）
			if (AChuandiBossCharacter* Boss = Cast<AChuandiBossCharacter>(Candidate))
			{
				if (UBreakMeterComponent* Meter = Boss->GetBreakMeter())
				{
					Meter->AddGauge(Tuning ? Tuning->BreakGaugeOnTrap : 15.f);
				}
			}

			UE_LOG(LogChuandi, Log, TEXT("[Trap] 触发！%s 被冰冻 3s（距离 %.0fcm）"),
				*GetNameSafe(Candidate), Dist);

			Trap->Destroy();
			break;
		}
	}
}
