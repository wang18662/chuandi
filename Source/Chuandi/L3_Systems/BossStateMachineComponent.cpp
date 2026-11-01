#include "L3_Systems/BossStateMachineComponent.h"

#include "L0_Core/ChuandiLog.h"
#include "L0_Core/ChuandiNativeTags.h"
#include "L1_Framework/ChuandiTuningData.h"
#include "L2_Entities/Boss/ChuandiBossCharacter.h"
#include "L2_Entities/World/CoverActor.h"
#include "L3_Systems/BossPerceptionComponent.h"
#include "L3_Systems/GAS/ChuandiAttributeSet.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

UBossStateMachineComponent::UBossStateMachineComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// 决策不需要每帧：5Hz 足够，且省性能
	PrimaryComponentTick.TickInterval = 0.2f;
}

void UBossStateMachineComponent::SetState(EBossState NewState)
{
	if (CurrentState == NewState)
	{
		return;
	}

	UE_LOG(LogChuandi, Log, TEXT("[FSM] %s → %s"),
		*UEnum::GetValueAsString(CurrentState), *UEnum::GetValueAsString(NewState));

	CurrentState = NewState;
}

void UBossStateMachineComponent::NotifyBreakTriggered()
{
	if (CurrentState == EBossState::Dead)
	{
		return;
	}

	bCasting = false;

	AChuandiBossCharacter* Boss = Cast<AChuandiBossCharacter>(GetOwner());
	if (Boss)
	{
		Boss->StopMoving();
	}

	SetState(EBossState::Broken);

	// 破防期间如果正在施法，把它取消掉（石头/红圈也要跟着消失）
	if (Boss && Boss->GetAbilitySystemComponent())
	{
		Boss->GetAbilitySystemComponent()->CancelAllAbilities();
	}
}

void UBossStateMachineComponent::NotifyDamaged(AActor* Attacker)
{
	if (!Attacker)
	{
		return;
	}

	AChuandiBossCharacter* Boss = Cast<AChuandiBossCharacter>(GetOwner());
	if (!Boss)
	{
		return;
	}

	if (UBossPerceptionComponent* Perception = Boss->GetPerception())
	{
		Perception->ForceAwareness(Attacker->GetActorLocation());
	}

	// 正在施法/已被控/已死时不打断，只更新认知
	if (CurrentState == EBossState::Dead || bCasting)
	{
		return;
	}

	if (CurrentState != EBossState::Chase)
	{
		UE_LOG(LogChuandi, Log, TEXT("[Boss] 受击唤醒 → Chase（攻击者 %s）"), *GetNameSafe(Attacker));
		SetState(EBossState::Chase);
	}
}

void UBossStateMachineComponent::NotifyCastFinished()
{
	bCasting = false;
	CastingSince = 0.0;

	// ★ 不要在这里同步 Evaluate（11-01 定位到的"命中后站桩"根因）：
	//   本函数是技能**在自己 EndAbility 之前**调的，那一刻技能还是 Active 状态。
	//   Evaluate 会立刻重新选招——如果玩家就在近战范围内，会再次选中同一个技能，
	//   而 TryActivateAbilityByClass 对一个仍在运行的实例必然失败 → bCasting 卡在 true、
	//   场上没有任何技能在跑 → Boss 从此站着不动。
	//   命中时玩家一定贴脸（所以必复现）；打空时玩家在范围外（所以不复现）——现象完全对得上。
	//   改为：只清标记，下一拍 5Hz Tick 自然决策，最多晚 0.2s。
}

bool UBossStateMachineComponent::UpdateSkillTier(float HealthRatio, double Now, AChuandiBossCharacter* Boss)
{
	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	if (!Tuning || !Boss)
	{
		return false;
	}

	int32 Desired = 0;
	if (HealthRatio <= Tuning->Tier3HealthRatio)
	{
		Desired = 3;
	}
	else if (HealthRatio <= Tuning->Tier2HealthRatio)
	{
		Desired = 2;
	}
	else if (HealthRatio <= Tuning->Tier1HealthRatio || ChaseAccum >= Tuning->Tier1ChaseTime)
	{
		Desired = 1;
	}

	if (Desired <= CurrentTier)
	{
		return false;
	}

	// Tier3 的前提：场上至少 2 根柱子（全屏秒杀的唯一解法是柱子，没柱子就放 = 必死）
	if (Desired >= 3)
	{
		int32 PillarNum = 0;
		for (TActorIterator<AChuandiCoverActor> It(GetWorld()); It; ++It)
		{
			++PillarNum;
		}
		if (PillarNum < 2)
		{
			Desired = 2;
		}
		if (Desired <= CurrentTier)
		{
			return false;
		}
	}

	// 一次只升一档：跳两级时下一拍再解锁，保证"一次只学一个新操作"
	++CurrentTier;

	FString CueName;
	int32 Forced = 0;
	if (CurrentTier == 1)
	{
		CueName = TEXT("新技能：牵引");
		Forced = 1;
	}
	else if (CurrentTier == 2)
	{
		CueName = TEXT("新技能：遁地 / 火焰");
		Forced = 2;
	}
	else
	{
		CueName = TEXT("新技能：全屏秒杀 / 冲撞");
		Forced = 3;
	}

	// 解锁提示三件套：停 1.5s + 闪金 + 屏幕中央技能名（07 §3.3-C）
	Boss->ApplyStagger(Tuning->UnlockStagger);
	Boss->FlashColor(FLinearColor(1.f, 0.85f, 0.30f, 1.f), 0.6f);
	Boss->ShowUnlockCue(CueName, Tuning->UnlockCueTime);

	// 强制"下一招就是刚解锁的那一招"，并把其它技能冷却推后，避免两个新招同时来
	ForcedSkill = Forced;
	const double Delay = Now + Tuning->UnlockOtherSkillDelay;
	NextWaveTime = Delay;
	NextMeteorTime = Delay;
	NextMeleeTime = Delay;
	NextPullTime = Delay;
	NextFlameTime = Delay;
	NextChargeTime = Delay;
	NextInstaKillTime = Delay;

	UE_LOG(LogChuandi, Log, TEXT("[Tier] 解锁 Tier%d（%s），Boss 停顿 %.1fs"),
		CurrentTier, *CueName, Tuning->UnlockStagger);
	return true;
}

void UBossStateMachineComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	Evaluate();
}

void UBossStateMachineComponent::Evaluate()
{
	AChuandiBossCharacter* Boss = Cast<AChuandiBossCharacter>(GetOwner());
	if (!Boss)
	{
		return;
	}

	UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	// ① 死亡：永久停机
	if (ASC->HasMatchingGameplayTag(ChuandiTags::State_Dead.GetTag()))
	{
		bCasting = false;
		SetState(EBossState::Dead);
		return;
	}

	// ② 破防 / 冰冻：被控就停手。tag 由 GE 授予，GE 到期 tag 自动消失 → 状态自动恢复，
	//    这里不需要自己记时长，这就是 A12「tag 驱动 FSM」的意思。
	const bool bBroken = ASC->HasMatchingGameplayTag(ChuandiTags::State_Broken.GetTag());
	const bool bFrozen = ASC->HasMatchingGameplayTag(ChuandiTags::State_Frozen.GetTag());

	if (bBroken || bFrozen)
	{
		bCasting = false;
		Boss->StopMoving();
		SetState(bFrozen ? EBossState::Frozen : EBossState::Broken);
		return;
	}

	if (CurrentState == EBossState::Broken || CurrentState == EBossState::Frozen)
	{
		// 07 §3.2：破防 → **追击**（不是待机）。之前无条件回 Idle，导致
		// 破防结束后 Boss 站着不动，要靠"受击唤醒"才恢复。
		const UBossPerceptionComponent* ExitPerception = Boss->GetPerception();
		SetState((ExitPerception && ExitPerception->IsTargetConfirmed())
			? EBossState::Chase : EBossState::Idle);
	}

	// 撞碎碎石后的 0.5s 停顿中：不移动、不攻击（07 §3.3-B），但仍可被玩家攻击
	if (Boss->IsStaggered())
	{
		return;
	}

	// 遁地结束 → 回追击/待机
	if (CurrentState == EBossState::Burrowed && !bCasting)
	{
		const UBossPerceptionComponent* BurrowPerception = Boss->GetPerception();
		const bool bStillSeenAfterBurrow = BurrowPerception && BurrowPerception->IsTargetConfirmed();
		SetState(bStillSeenAfterBurrow ? EBossState::Chase : EBossState::Idle);
	}

	// ★ 施法结束的出口（之前漏了这一步：Casting 变成死状态，
	//   Boss 放完第一次声波就永远卡在 Casting，既不追也不打）
	if (CurrentState == EBossState::Casting && !bCasting)
	{
		const UBossPerceptionComponent* OutletPerception = Boss->GetPerception();
		const bool bStillSeen = OutletPerception && OutletPerception->IsTargetConfirmed();
		SetState(bStillSeen ? EBossState::Chase : EBossState::Idle);
	}

	// ③ 正在施法：站住，等 Ability 回调
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;

	if (bCasting)
	{
		// ★ 卡死兜底：全场最长的技能是全屏秒杀（10s）。超过 12s 还没收到 NotifyCastFinished，
		//   说明某个技能没能回调（激活失败/被打断没通知），Boss 会永远停在"站立不动、不放技能"。
		//   这里强制放行，并在日志里留下当时的状态，方便定位是哪个技能。
		if (CastingSince <= 0.0)
		{
			CastingSince = Now;
		}
		else if (Now - CastingSince > 12.0)
		{
			UE_LOG(LogChuandi, Warning,
				TEXT("[FSM] 施法卡死 %.1fs（state=%d）→ 强制放行，请回看上一个技能的日志"),
				Now - CastingSince, static_cast<int32>(GetState()));
			bCasting = false;
			CastingSince = 0.0;
			Boss->StopMoving();
			SetState(EBossState::Chase);
		}

		Boss->StopMoving();
		return;
	}

	CastingSince = 0.0;

	// 兜底：场上有技能还在跑（重入窗口）时，本拍不选招，避免"激活失败 + bCasting 卡死"
	{
		bool bAnyAbilityActive = false;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.IsActive())
			{
				bAnyAbilityActive = true;
				break;
			}
		}
		if (bAnyAbilityActive)
		{
			return;
		}
	}

	// ★ 阶段判定（02 §6.4 / 10 §第二优先）
	//   100–60%：常态　≤40%：陨石加密　<20%：遁地解禁（三种技能全开）
	const UChuandiAttributeSet* Attr = Boss->GetChuandiAttributeSet();
	const float MaxHP = Attr ? FMath::Max(1.f, Attr->GetMaxHealth()) : 1.f;
	const float Ratio = Attr ? Attr->GetHealth() / MaxHP : 1.f;

	const UChuandiTuningData* PhaseTuning = UChuandiTuningData::Get();
	const float DenseThreshold = PhaseTuning ? PhaseTuning->MeteorDenseThreshold : 0.4f;
	const float EnrageThreshold = PhaseTuning ? PhaseTuning->EnrageThreshold : 0.2f;
	const float BurrowThreshold = PhaseTuning ? PhaseTuning->BurrowThreshold : 0.6f;
	const float BurrowCd = PhaseTuning ? PhaseTuning->BurrowCooldown : 15.f;
	const bool bEnraged = Ratio <= EnrageThreshold;

	// 07 §3.3-C：按血量/追击时长推进技能解锁；刚升档就 return（这一拍不施法）
	if (UpdateSkillTier(Ratio, Now, Boss))
	{
		return;
	}

	// 阶段切换只闪一次金（10 §第二优先"阶段切换提示"；音效/震屏不在本轮范围）
	if (!bDensePhaseAnnounced && Ratio <= DenseThreshold)
	{
		bDensePhaseAnnounced = true;
		Boss->FlashColor(FLinearColor(1.f, 0.83f, 0.28f, 1.f), 0.35f);
		UE_LOG(LogChuandi, Log, TEXT("[Phase] 血量 %.0f%% ≤ %.0f%% → 40%% 阶段：陨石 6→8 圈、存在 3.0→2.5s"),
			Ratio * 100.f, DenseThreshold * 100.f);
	}
	if (!bEnrageAnnounced && Ratio <= EnrageThreshold)
	{
		bEnrageAnnounced = true;
		Boss->FlashColor(FLinearColor(1.f, 0.55f, 0.2f, 1.f), 0.5f);
		UE_LOG(LogChuandi, Log, TEXT("[Phase] 血量 %.0f%% ≤ %.0f%% → 狂暴：遁地进入常规冷却（%.0fs）"),
			Ratio * 100.f, EnrageThreshold * 100.f, BurrowCd);
	}

	// ★ 遁地（07 §3.2 / 02 §6.4）：≤60% 触发一次；<20% 狂暴期按冷却反复
	//   放在感知之前：它优先于普通施法，且进入后 bCasting 会挡住其它分支
	if (CurrentTier >= 2 && !bCasting && (!bBurrowedOnce || bEnraged)
		&& Ratio <= BurrowThreshold && Now >= NextBurrowTime)
	{
		bBurrowedOnce = true;
		NextBurrowTime = Now + BurrowCd;
		bCasting = true;
		Boss->StopMoving();
		SetState(EBossState::Burrowed);
		Boss->TryBurrow();
		UE_LOG(LogChuandi, Log, TEXT("[FSM] 血量 %.0f%% ≤ %.0f%% → 遁地（%s）"),
			Ratio * 100.f, BurrowThreshold * 100.f,
			bEnraged ? TEXT("狂暴期可重复") : TEXT("一次性"));
		return;
	}

	// ④ 感知 → 状态迁移
	//    V5：连续看见 0.3s → Idle 转 Chase（就是"引诱成功"的那一刻）
	//    V6：失去视野 4s → Chase 回 Idle
	UBossPerceptionComponent* Perception = Boss->GetPerception();
	const bool bConfirmed = Perception && Perception->IsTargetConfirmed();
	const bool bForgotten = Perception && Perception->IsTargetForgotten(Now);

	if (CurrentState == EBossState::Idle && bConfirmed)
	{
		SetState(EBossState::Chase);
	}
	else if (CurrentState == EBossState::Chase && bForgotten)
	{
		SetState(EBossState::Idle);
	}

	// 诊断：每 2 秒无条件打一次完整决策上下文（不依赖是否进入 Chase）
	DebugAccum += GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	if (DebugAccum >= 2.f)
	{
		DebugAccum = 0.f;
		const APawn* DbgPlayer = UGameplayStatics::GetPlayerPawn(this, 0);
		UE_LOG(LogChuandi, Log,
			TEXT("[BossDbg] state=%s tick=%d suspicion=%.2f los=%d dist=%.0f casting=%d"),
			*UEnum::GetValueAsString(CurrentState),
			Boss->IsActorTickEnabled() ? 1 : 0,
			Perception ? Perception->GetSuspicion() : -1.f,
			Perception && Perception->HasLineOfSight() ? 1 : 0,
			DbgPlayer ? FVector::Dist2D(Boss->GetActorLocation(), DbgPlayer->GetActorLocation()) : -1.f,
			bCasting ? 1 : 0);
	}

	// ⑤ 行为
	if (CurrentState == EBossState::Chase)
	{
		// 07 §3.3-C 的 Tier1 时间兜底：只在追击状态累计
		ChaseAccum += World ? World->GetDeltaSeconds() : 0.2f;

		// 只更新目标点，移动本身在 Boss 的 Tick 里每帧做（5Hz 决策不能直接驱动位移）
		Boss->SetChaseTarget(Perception->GetLastKnownLocation());

		const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
		if (!Player)
		{
			return;
		}

		const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
		const float CastRange = Tuning ? Tuning->BossCastRange : 1000.f;
		const float WaveCd = Tuning ? Tuning->WaveCooldown : 8.f;
		const float MeteorCd = Tuning ? Tuning->MeteorCooldown : 12.f;
		const float MeleeRange = Tuning ? Tuning->MeleeRange : 200.f;
		const float MeleeCooldown = Tuning ? Tuning->MeleeCooldown : 1.2f;

		const float Dist = FVector::Dist(Boss->GetActorLocation(), Player->GetActorLocation());

		// 07 §3.3：贴身优先近战（2m / 1.2s 冷却）
		if (Dist <= MeleeRange && Now >= NextMeleeTime)
		{
			Boss->StopMoving();
			bCasting = true;
			NextMeleeTime = Now + MeleeCooldown;
			SetState(EBossState::Casting);
			Boss->TryMelee();
			return;
		}

		// ── 全屏秒杀（10-29）：优先级最高。场上没有柱子就不放 —— 一根都没有时放=必死，不公平 ──
		int32 PillarCount = 0;
		for (TActorIterator<AChuandiCoverActor> It(World); It; ++It)
		{
			++PillarCount;
		}

		const float InstaCd = Tuning ? Tuning->InstaKillCooldown : 35.f;
		const int32 MinPillars = Tuning ? Tuning->InstaKillMinPillars : 1;

		// 07 §3.3-C：刚解锁的那一招必须是"下一个释放的"（教学保证）。
		// 条件不满足（距离/LOS/柱子不够）就放弃强制，下一拍正常选招——不要死等。
		if (ForcedSkill != 0)
		{
			const int32 Skill = ForcedSkill;
			ForcedSkill = 0;

			if (Skill == 1)
			{
				const float PullMinR = Tuning ? Tuning->PullMinRange : 250.f;
				const float PullMaxR = Tuning ? Tuning->PullMaxRange : 1400.f;
				if (Perception && Perception->HasLineOfSight() && Dist >= PullMinR && Dist <= PullMaxR)
				{
					Boss->StopMoving();
					bCasting = true;
					NextPullTime = Now + (Tuning ? Tuning->PullCooldown : 18.f);
					SetState(EBossState::Casting);
					Boss->TryPull();
					return;
				}
			}
			else if (Skill == 2 && Ratio <= BurrowThreshold)
			{
				bBurrowedOnce = true;
				bCasting = true;
				NextBurrowTime = Now + (Tuning ? Tuning->BurrowCooldown : 15.f);
				Boss->StopMoving();
				SetState(EBossState::Burrowed);
				Boss->TryBurrow();
				return;
			}
			else if (Skill == 3 && PillarCount >= 1)
			{
				Boss->StopMoving();
				bCasting = true;
				NextInstaKillTime = Now + (Tuning ? Tuning->InstaKillCooldown : 35.f);
				SetState(EBossState::Casting);
				Boss->TryInstaKill();
				return;
			}
		}
		if (CurrentTier >= 3 && PillarCount >= MinPillars && Now >= NextInstaKillTime)
		{
			Boss->StopMoving();
			bCasting = true;
			NextInstaKillTime = Now + InstaCd;
			SetState(EBossState::Casting);
			Boss->TryInstaKill();
			return;
		}

		// ── 冲撞破柱（10-29）：撞 3 根直接破防，是现在的主破防来源 ──
		const float ChargeCd = Tuning ? Tuning->ChargeCooldown : 14.f;
		if (CurrentTier >= 3 && PillarCount > 0 && Now >= NextChargeTime)
		{
			Boss->StopMoving();
			bCasting = true;
			NextChargeTime = Now + ChargeCd;
			SetState(EBossState::Casting);
			Boss->TryCharge();
			return;
		}

		// ── 牵引（10-29）：中距离把玩家拉过来，横扫逼弹反 ──
		// 放在声波/陨石之前：它是"唯一可弹反的近身技能"，被抢优先级就失去教学作用
		const float PullMin = Tuning ? Tuning->PullMinRange : 250.f;
		const float PullMax = Tuning ? Tuning->PullMaxRange : 1400.f;
		const float PullCd = Tuning ? Tuning->PullCooldown : 18.f;
		// 必须有视线：否则会隔着碎石/墙把玩家"拉穿"几何体，看起来像 bug
		const bool bPullHasLOS = Perception && Perception->HasLineOfSight();
		if (CurrentTier >= 1 && bPullHasLOS && Dist >= PullMin && Dist <= PullMax && Now >= NextPullTime)
		{
			Boss->StopMoving();
			bCasting = true;
			NextPullTime = Now + PullCd;
			SetState(EBossState::Casting);
			Boss->TryPull();
			return;
		}

		// ── 火焰（10-29）：脚下 + 两侧铺 3 片火，逼玩家离开原位 ──
		const float FlameCd = Tuning ? Tuning->FlameCooldown : 10.f;
		if (CurrentTier >= 2 && Now >= NextFlameTime)
		{
			Boss->StopMoving();
			bCasting = true;
			NextFlameTime = Now + FlameCd;
			SetState(EBossState::Casting);
			Boss->TryFlame();
			return;
		}

		// 中距离：声波 8s / 陨石 12s，**各自独立冷却**（交接 §1）
		// ★ 之前两个技能共用一个 NextCastTime（都用 WaveCooldown 计时），
		//   导致 MeteorCooldown=12 从来没被读过，陨石实际按 8s 放 →
		//   破防机会比设计多 50%，150s 里能多打出一次破防。
		if (Dist <= CastRange)
		{
			// ★ 声波有最小距离（10-29）：贴身放声波时环 0.2s 就到人、三波连跳来不及，
			//   等于"不可躲"。被牵引拉到 3m 后如果马上接声波，玩家会觉得被耍。
			//   近身交给近战 / 牵引，声波只在 6m 外放。
			const float WaveMin = Tuning ? Tuning->WaveMinRange : 600.f;
			const bool bWaveReady = Now >= NextWaveTime && Dist >= WaveMin;
			const bool bMeteorReady = Now >= NextMeteorTime;

			if (bWaveReady || bMeteorReady)
			{
				// 都就绪 → 交替（口径简单、不会连放同一招）；只有一个就绪 → 就放它
				const bool bCastMeteor = (bWaveReady && bMeteorReady) ? bNextSkillIsMeteor : bMeteorReady;

				Boss->StopMoving();
				bCasting = true;
				SetState(EBossState::Casting);

				if (bCastMeteor)
				{
					NextMeteorTime = Now + MeteorCd;
					bNextSkillIsMeteor = false;
					Boss->TryCastMeteor();
				}
				else
				{
					NextWaveTime = Now + WaveCd;
					bNextSkillIsMeteor = true;
					Boss->TryCastWave();
				}
			}
		}

		// ★ Chase 必须在这里 return：下面那句 StopMoving 是"巡逻/待机"用的，
		//   追击时若落到那里会把追击目标清掉（表现为 Boss 走一半突然站住）。
		return;
	}

	// 巡逻：站着不动，等被看见。
	// ★ 这里故意不再"隔墙按距离放技能"——看不见就不该打，否则引诱失去意义。
	Boss->StopMoving();
}
