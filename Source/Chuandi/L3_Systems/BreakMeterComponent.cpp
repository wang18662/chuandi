#include "L3_Systems/BreakMeterComponent.h"

#include "L0_Core/ChuandiLog.h"
#include "L0_Core/ChuandiNativeTags.h"
#include "L1_Framework/ChuandiGameMode.h"
#include "L1_Framework/ChuandiTuningData.h"
#include "L2_Entities/Boss/ChuandiBossCharacter.h"
#include "L3_Systems/BossStateMachineComponent.h"
#include "L3_Systems/GAS/ChuandiBreakEffect.h"
#include "L3_Systems/GAS/ChuandiCombatLibrary.h"
#include "Kismet/GameplayStatics.h"

UBreakMeterComponent::UBreakMeterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBreakMeterComponent::BeginPlay()
{
	Super::BeginPlay();

	// 构造期读不到 DataAsset（CDO 阶段），放 BeginPlay
	if (const UChuandiTuningData* Tuning = UChuandiTuningData::Get())
	{
		MaxMeter = Tuning->BreakMeterMax;
	}

	Meter = 0.f;
	OnMeterChanged.Broadcast(Meter, MaxMeter);
}

void UBreakMeterComponent::AddGauge(float Amount)
{
	if (Amount <= 0.f)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// 破防期间不续条（07 B-04），否则条会溢出、破防结束立刻又满
	if (UChuandiCombatLibrary::HasTag(Owner, ChuandiTags::State_Broken.GetTag()))
	{
		return;
	}

	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float Max = Tuning ? Tuning->BreakMeterMax : MaxMeter;
	MaxMeter = Max;

	Meter = FMath::Clamp(Meter + Amount, 0.f, Max);
	OnMeterChanged.Broadcast(Meter, Max);

	UE_LOG(LogChuandi, Log, TEXT("[Break] 破防条 +%.0f → %.0f / %.0f"),
		Amount, Meter, Max);

	if (Meter >= Max)
	{
		TriggerBreak();
	}
}

void UBreakMeterComponent::TriggerBreak()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float Duration = Tuning ? Tuning->BreakDuration : 5.f;

	// 触发即清零（07 B-03）
	Meter = 0.f;
	OnMeterChanged.Broadcast(Meter, MaxMeter);

	// ① GE 负责"受伤 ×15 + 3 秒后自动恢复"（时长由 spec 覆盖，tag 挂在 spec 上）
	UChuandiCombatLibrary::ApplyStatusGEWithTags(Owner, Owner,
		UChuandiBreakEffect::StaticClass(), ChuandiTags::State_Broken.GetTag(), 1.f, Duration);

	// ② 状态立刻切过去（B-01：条满到破防 ≤1 帧，不能等 5Hz 的下一拍）
	if (AChuandiBossCharacter* Boss = Cast<AChuandiBossCharacter>(Owner))
	{
		if (UBossStateMachineComponent* FSM = Boss->GetStateMachine())
		{
			FSM->NotifyBreakTriggered();
		}
	}

	// 结算统计：破防次数
	if (AChuandiGameMode* GM = Cast<AChuandiGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->RegisterBreak();
	}

	// ③ 破防进入瞬间：0.1s 现实顿帧 + Boss 闪金（07 §1.8，11:53 版）
	//    ★ 不再做全局时间膨胀：0.05 倍下"游戏内 5 秒 = 现实 100 秒"，玩家会被困在慢动作里。
	UChuandiCombatLibrary::HitStop(GetWorld(), Tuning ? Tuning->BreakHitStopRealTime : 0.1f, 0.01f);

	if (AChuandiBossCharacter* FlashTarget = Cast<AChuandiBossCharacter>(Owner))
	{
		FlashTarget->FlashColor(FLinearColor(1.f, 0.83f, 0.28f, 1.f), 0.25f);
	}

	UE_LOG(LogChuandi, Log, TEXT("[Break] 条满 → 破防 %.1fs，受伤 x%.0f，顿帧 0.10s + Boss 闪金（不做时间膨胀）"),
		Duration, Tuning ? Tuning->BreakDamageMultiplier : 12.f);
}
