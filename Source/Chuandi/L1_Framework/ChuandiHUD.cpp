#include "L1_Framework/ChuandiHUD.h"

#include "L0_Core/ChuandiNativeTags.h"
#include "L1_Framework/ChuandiGameMode.h"
#include "L1_Framework/ChuandiTuningData.h"
#include "L2_Entities/Boss/ChuandiBossCharacter.h"
#include "L2_Entities/Player/ChuandiPlayerCharacter.h"
#include "L2_Entities/World/CoverActor.h"
#include "L3_Systems/BossPerceptionComponent.h"
#include "L3_Systems/BossStateMachineComponent.h"
#include "L3_Systems/BreakMeterComponent.h"
#include "L3_Systems/GAS/ChuandiAbilitySystemComponent.h"
#include "L3_Systems/GAS/ChuandiAttributeSet.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "CanvasItem.h"
#include "EngineUtils.h"
#include "Engine/Canvas.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RenderUtils.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FLinearColor ParryCyan(0.15f, 0.95f, 0.95f, 1.f);
	const FLinearColor DangerRed(0.95f, 0.2f, 0.2f, 1.f);
}

void AChuandiHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	const float ScreenW = Canvas->SizeX;
	const float ScreenH = Canvas->SizeY;
	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	APawn* PlayerPawn = GetOwningPawn();
	UAbilitySystemComponent* PlayerASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerPawn);
	const UChuandiAttributeSet* PlayerAttr = PlayerASC ? PlayerASC->GetSet<UChuandiAttributeSet>() : nullptr;

	// ── 反馈 ①：弹反窗口开启中（教玩家什么时候按） ──
	if (const UChuandiAbilitySystemComponent* ChuandiASC = Cast<UChuandiAbilitySystemComponent>(PlayerASC))
	{
		if (ChuandiASC->IsParryWindowOpen(Now))
		{
			const float BarW = 280.f;
			const float BarH = 12.f;
			const float BarX = ScreenW * 0.5f - BarW * 0.5f;
			const float BarY = ScreenH - 150.f;

			// 剩余时间占比：条会从右往左缩，玩家能看出"窗口正在结束"
			const double Start = ChuandiASC->GetParryWindowStart();
			const double End = ChuandiASC->GetParryWindowEnd();
			const double Total = FMath::Max(0.01, End - Start);
			const float Ratio = FMath::Clamp(static_cast<float>((End - Now) / Total), 0.f, 1.f);

			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.50f), BarX - 4.f, BarY - 4.f, BarW + 8.f, BarH + 8.f);
			DrawRect(FLinearColor(0.07f, 0.07f, 0.08f, 0.92f), BarX, BarY, BarW, BarH);
			DrawRect(ParryCyan, BarX, BarY, BarW * Ratio, BarH);
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.25f), BarX, BarY, BarW * Ratio, BarH * 0.35f);

			const FLinearColor WinEdge(0.78f, 0.78f, 0.78f, 0.22f);
			DrawRect(WinEdge, BarX, BarY, BarW, 1.f);
			DrawRect(WinEdge, BarX, BarY + BarH - 1.f, BarW, 1.f);

			// 标题居中（原来手填 +78 的偏移，换字号就歪）
			const FString WinLabel = TEXT("PARRY WINDOW");
			float WinW = 0.f;
			float WinH = 0.f;
			GetTextSize(WinLabel, WinW, WinH, nullptr, 1.3f);
			DrawText(WinLabel, ParryCyan, BarX + (BarW - WinW) * 0.5f, BarY - 24.f, nullptr, 1.3f);
		}
	}

	// ── 反馈 ③：玩家掉血 → 边缘泛红（下降沿触发） ──
	if (PlayerAttr)
	{
		const float HP = PlayerAttr->GetHealth();
		if (LastPlayerHealth >= 0.f && HP < LastPlayerHealth - KINDA_SMALL_NUMBER)
		{
			HitFlashRemaining = 0.35f;
		}
		LastPlayerHealth = HP;
	}

	if (HitFlashRemaining > 0.f)
	{
		HitFlashRemaining -= DeltaSeconds;
		const float Alpha = FMath::Clamp(HitFlashRemaining / 0.35f, 0.f, 1.f) * 0.35f;
		const float Edge = 40.f;
		DrawRect(FLinearColor(1.f, 0.f, 0.f, Alpha), 0.f, 0.f, ScreenW, Edge);
		DrawRect(FLinearColor(1.f, 0.f, 0.f, Alpha), 0.f, ScreenH - Edge, ScreenW, Edge);
		DrawRect(FLinearColor(1.f, 0.f, 0.f, Alpha), 0.f, 0.f, Edge, ScreenH);
		DrawRect(FLinearColor(1.f, 0.f, 0.f, Alpha), ScreenW - Edge, 0.f, Edge, ScreenH);
	}

	// ── 准星：细环 + 中心点（比 4px 白方块在亮地面上更容易辨认）──
	{
		const float CrossX = ScreenW * 0.5f;
		const float CrossY = ScreenH * 0.5f;
		DrawRing(CrossX, CrossY, 7.f, 1.f, FLinearColor(1.f, 1.f, 1.f, 0.45f), 24);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), CrossX - 2.f, CrossY - 2.f, 4.f, 4.f);
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.90f), CrossX - 1.f, CrossY - 1.f, 2.f, 2.f);
	}

	// ── 血条 ──
	DrawHealthBarFor(PlayerPawn, 40.f, ScreenH - 90.f, 420.f, 24.f,
		FLinearColor(0.15f, 0.75f, 0.30f, 0.95f), TEXT("玩家"));

	// 传递物数量（07 §2.5：右下角）
	if (const AChuandiPlayerCharacter* PlayerChar = Cast<AChuandiPlayerCharacter>(PlayerPawn))
	{
		const FString ChargeText = FString::Printf(TEXT("传递物  x %d"), PlayerChar->GetTrapCharges());
		float ChargeW = 0.f;
		float ChargeH = 0.f;
		GetTextSize(ChargeText, ChargeW, ChargeH, nullptr, 1.2f);
		const float PillX = ScreenW - 56.f - ChargeW - 20.f;
		const float PillY = ScreenH - 132.f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), PillX, PillY, ChargeW + 20.f, ChargeH + 10.f);
		DrawRect(FLinearColor(0.83f, 0.63f, 0.09f, 0.9f), PillX, PillY, 2.f, ChargeH + 10.f);
		DrawText(ChargeText, FLinearColor(1.f, 0.88f, 0.55f, 1.f), PillX + 12.f, PillY + 5.f, nullptr, 1.2f);
	}

	// ── 左侧目标栏（11-01）：把纸面"任务流程链"演示化——评委随时知道现在该做什么 ──
	//   刻意只给"目标 + 当前一步 + 进度"三行，不做任务清单：单场 Boss 战做 checklist 会像手游。
	if (PlayerPawn)
	{
		const AChuandiBossCharacter* GoalBoss = nullptr;
		if (const AChuandiGameMode* GoalGM = Cast<AChuandiGameMode>(UGameplayStatics::GetGameMode(this)))
		{
			GoalBoss = Cast<AChuandiBossCharacter>(GoalGM->GetPrimaryTarget());
		}

		int32 PillarNum = 0;
		for (TActorIterator<AChuandiCoverActor> It(GetWorld()); It; ++It)
		{
			++PillarNum;
		}

		// 当前一步：当前 Boss 技能 + 玩家该做什么（左侧栏展示）
		auto SkillToAction = [](const FString& S) -> FString
		{
			if (S == TEXT("全屏秒杀")) return TEXT("站上碎石柱躲开（柱子替你挡）");
			if (S == TEXT("冲撞"))   return TEXT("跑到柱子后，让它撞柱（3根破防）");
			if (S == TEXT("牵引"))   return TEXT("被锁住，前摇结束按 Q 弹反横扫");
			if (S == TEXT("火焰"))   return TEXT("保持走位躲开火焰");
			if (S == TEXT("横扫普攻")) return TEXT("走位躲开横扫");
			if (S == TEXT("声波"))   return TEXT("进黄圈按 Q 弹反");
			if (S == TEXT("陨石"))   return TEXT("看到预警圈就按 Q 弹反");
			if (S == TEXT("遁地"))   return TEXT("听脚步/看裂纹，准备闪避突进");
			return TEXT("看预警，准备应对");
		};

		const bool PlayerRooted = (Cast<AChuandiPlayerCharacter>(PlayerPawn)
			&& Cast<AChuandiPlayerCharacter>(PlayerPawn)->IsForceRooted());
		const UBossStateMachineComponent* BSM = GoalBoss ? GoalBoss->GetStateMachine() : nullptr;
		const EBossState BossState = BSM ? BSM->GetState() : EBossState::Idle;

		FString SkillName = TEXT("待机");
		FString PlayerAction = TEXT("观察 Boss 动作");
		if (GoalBoss)
		{
			const FString CastName = GoalBoss->GetCurrentSkillName();
			if (BossState == EBossState::Dead)
			{
				SkillName = TEXT("已击败"); PlayerAction = TEXT("战斗结束");
			}
			else if (BossState == EBossState::Broken)
			{
				SkillName = TEXT("破防"); PlayerAction = TEXT("全力输出，打满破防条");
			}
			else if (BossState == EBossState::Frozen)
			{
				SkillName = TEXT("冻住"); PlayerAction = TEXT("趁机输出");
			}
			else if (!CastName.IsEmpty())
			{
				SkillName = CastName; PlayerAction = SkillToAction(CastName);
			}
			else if (PlayerRooted)
			{
				SkillName = TEXT("牵引·横扫"); PlayerAction = TEXT("前摇结束按 Q 弹反");
			}
			else if (BossState == EBossState::Chase)
			{
				SkillName = TEXT("追击"); PlayerAction = TEXT("保持移动，找弹反窗口");
			}
		}

		const float PanelX = 34.f;
		const float PanelY = ScreenH * 0.28f;
		const float PanelW = 360.f;
		const float PanelH = 122.f;

		// 统一面板语言：黑底 + 四边 1px 描边 + 左侧金色强调条
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), PanelX, PanelY, PanelW, PanelH);
		const FLinearColor PanelEdge(0.78f, 0.78f, 0.78f, 0.20f);
		DrawRect(PanelEdge, PanelX, PanelY, PanelW, 1.f);
		DrawRect(PanelEdge, PanelX, PanelY + PanelH - 1.f, PanelW, 1.f);
		DrawRect(PanelEdge, PanelX, PanelY, 1.f, PanelH);
		DrawRect(PanelEdge, PanelX + PanelW - 1.f, PanelY, 1.f, PanelH);
		DrawRect(FLinearColor(0.83f, 0.63f, 0.09f, 0.95f), PanelX, PanelY, 3.f, PanelH);

		const float ColLabel = PanelX + 14.f;
		const float ColValue = PanelX + 96.f;
		// 行1：目标
		DrawText(TEXT("目标"), FLinearColor(0.83f, 0.63f, 0.09f, 1.f),
			ColLabel, PanelY + 8.f, nullptr, 1.1f);
		DrawText(TEXT("击败侦察型"), FLinearColor(0.92f, 0.92f, 0.92f, 1.f),
			ColValue, PanelY + 8.f, nullptr, 1.15f);
		// 行2：当前 Boss 技能（危险技能标红）
		DrawText(TEXT("Boss技能"), FLinearColor(0.83f, 0.63f, 0.09f, 1.f),
			ColLabel, PanelY + 38.f, nullptr, 1.1f);
		const bool bDanger = SkillName == TEXT("全屏秒杀") || SkillName == TEXT("冲撞") || SkillName == TEXT("牵引");
		DrawText(SkillName, bDanger ? FLinearColor(1.f, 0.35f, 0.30f, 1.f) : FLinearColor(0.55f, 0.85f, 1.f, 1.f),
			ColValue, PanelY + 38.f, nullptr, 1.25f);
		// 行3：玩家该做什么
		DrawText(TEXT("你要做"), FLinearColor(0.83f, 0.63f, 0.09f, 1.f),
			ColLabel, PanelY + 66.f, nullptr, 1.1f);
		DrawText(PlayerAction, FLinearColor(1.f, 0.86f, 0.45f, 1.f),
			ColValue, PanelY + 66.f, nullptr, 1.1f);
		// 行4：进度
		if (GoalBoss && GoalBoss->GetBreakMeter())
		{
			const int32 Percent = FMath::RoundToInt(GoalBoss->GetBreakMeter()->GetNormalized() * 100.f);
			DrawText(FString::Printf(TEXT("柱子 %d ｜ 破防 %d%%"), PillarNum, Percent),
				FLinearColor(0.65f, 0.65f, 0.65f, 1.f), ColLabel, PanelY + 96.f, nullptr, 1.0f);
		}
	}

	// ── 右下角技能栏（CHANGELOG 16:04~16:08 / 09_UI 规范）──
	DrawSkillBar(PlayerPawn, ScreenW, ScreenH, Now);

	// ── 禁锢提示（10-29 牵引）：这段时间不能移动，只能等横扫落点按 Q ──
	if (const AChuandiPlayerCharacter* RootedChar = Cast<AChuandiPlayerCharacter>(PlayerPawn))
	{
		if (RootedChar->IsForceRooted())
		{
			const FString RootText = TEXT("被禁锢  ——  前摇结束时按 Q 弹反");
			const float BoxW = 340.f;
			const float BoxX = ScreenW * 0.5f - BoxW * 0.5f;
			const float BoxY = ScreenH - 150.f;

			DrawRect(FLinearColor(0.10f, 0.05f, 0.16f, 0.75f), BoxX, BoxY, BoxW, 34.f);
			DrawRect(FLinearColor(0.65f, 0.30f, 0.95f, 0.95f), BoxX, BoxY, 3.f, 34.f);
			DrawText(RootText, FLinearColor(0.85f, 0.72f, 1.f, 1.f), BoxX + 14.f, BoxY + 8.f, nullptr, 1.2f);
		}
	}

	// ── 柱子交互提示（10-29）：对准柱子才出现，站在柱子上时提示"飞跃" ──
	if (const AChuandiPlayerCharacter* PlayerChar = Cast<AChuandiPlayerCharacter>(PlayerPawn))
	{
		if (PlayerChar->GetFocusedPillar() && !PlayerChar->IsClimbing())
		{
			const bool bOnPillar = PlayerChar->IsStandingOnPillar();
			const FString Prompt = bOnPillar
				? TEXT("F   飞跃到另一根柱子")
				: TEXT("F   跳上碎石柱");

			const float BoxW = 300.f;
			const float BoxX = ScreenW * 0.5f - BoxW * 0.5f;
			const float BoxY = ScreenH * 0.62f;

			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), BoxX, BoxY, BoxW, 40.f);
			DrawRect(FLinearColor(0.83f, 0.63f, 0.09f, 0.95f), BoxX, BoxY, 3.f, 40.f);
			DrawText(Prompt, FLinearColor(1.f, 0.90f, 0.55f, 1.f), BoxX + 16.f, BoxY + 10.f, nullptr, 1.35f);
		}
	}

	const AChuandiGameMode* GM = Cast<AChuandiGameMode>(UGameplayStatics::GetGameMode(this));
	if (!GM)
	{
		return;
	}

	AActor* Target = GM->GetPrimaryTarget();
	if (Target)
	{
		DrawHealthBarFor(Target, ScreenW * 0.5f - 300.f, 34.f, 600.f, 22.f,
			FLinearColor(0.85f, 0.15f, 0.15f, 0.95f), TEXT("主目标"));

		// ── 反馈 ②：Boss 进入破防的上升沿 → "PARRY!" ──
		UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
		const bool bBrokenNow = TargetASC
			&& TargetASC->HasMatchingGameplayTag(ChuandiTags::State_Broken.GetTag());

		if (bBrokenNow && !bWasBossBroken)
		{
			ParryFlashRemaining = 0.9f;
		}
		bWasBossBroken = bBrokenNow;

		// ── Boss 血量阶段提示（02 §6.4）：≤40% 陨石加密 / <20% 狂暴 ──
		if (const UChuandiAttributeSet* TargetAttr =
			TargetASC ? TargetASC->GetSet<UChuandiAttributeSet>() : nullptr)
		{
			const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
			const float MaxHP = FMath::Max(1.f, TargetAttr->GetMaxHealth());
			const float HPRatio = FMath::Clamp(TargetAttr->GetHealth() / MaxHP, 0.f, 1.f);
			const float EnrageAt = Tuning ? Tuning->EnrageThreshold : 0.2f;
			const float DenseAt = Tuning ? Tuning->MeteorDenseThreshold : 0.4f;

			const int32 Phase = (HPRatio <= EnrageAt) ? 3 : ((HPRatio <= DenseAt) ? 2 : 1);
			if (Phase > LastBossPhase)
			{
				PhaseAnnounceRemaining = 1.8f;
				PhaseAnnounceText = (Phase == 2) ? TEXT("PHASE 2   陨石加密") : TEXT("PHASE 3   狂暴");
			}
			LastBossPhase = Phase;
		}

		if (bBrokenNow)
		{
			DrawText(TEXT("BREAK"), ParryCyan, ScreenW * 0.5f + 196.f, 62.f, nullptr, 1.5f);
		}

		const bool bFrozenNow = TargetASC
			&& TargetASC->HasMatchingGameplayTag(ChuandiTags::State_Frozen.GetTag());
		if (bFrozenNow)
		{
			DrawText(TEXT("FROZEN"), FLinearColor(0.45f, 0.8f, 1.f, 1.f),
				ScreenW * 0.5f + 196.f, 62.f, nullptr, 1.5f);
		}

		// ── 全屏秒杀预警（10-29）：屏幕上方最醒目的一条，倒计时 + 一句话解法 ──
		if (const AChuandiBossCharacter* WarnBoss = Cast<AChuandiBossCharacter>(Target))
		{
			if (WarnBoss->IsInstaKillWarningActive())
			{
				const float Remaining = WarnBoss->GetInstaKillWarningRemaining();
				const float Pulse = 0.55f + 0.45f * FMath::Abs(FMath::Sin(Now * PI * 4.f));
				const float BarW = 760.f;
				const float BarX = ScreenW * 0.5f - BarW * 0.5f;

				DrawRect(FLinearColor(0.30f, 0.02f, 0.02f, 0.80f * Pulse), BarX, 88.f, BarW, 52.f);
				DrawRect(FLinearColor(1.f, 0.15f, 0.10f, 0.95f), BarX, 88.f, BarW, 3.f);

				DrawText(FString::Printf(TEXT("全屏秒杀    %.1f s"), Remaining),
					FLinearColor(1.f, 0.92f, 0.88f, 1.f), BarX + 20.f, 96.f, nullptr, 2.2f);
				DrawText(TEXT("对准碎石按 F 站上去（柱子会替你挡下这一击）"),
					FLinearColor(1.f, 0.74f, 0.36f, 1.f), BarX + 20.f, 152.f, nullptr, 1.3f);
			}
		}

		// ── 冲撞锁定（10-30）：5s 内把柱子摆到自己和 Boss 之间 ──
		if (const AChuandiBossCharacter* ChargeBoss = Cast<AChuandiBossCharacter>(Target))
		{
			if (ChargeBoss->IsChargeLockActive())
			{
				const float Remaining = ChargeBoss->GetChargeLockRemaining();
				const float Pulse = 0.60f + 0.40f * FMath::Abs(FMath::Sin(Now * PI * 6.f));
				const float BarW = 760.f;
				const float BarX = ScreenW * 0.5f - BarW * 0.5f;

				DrawRect(FLinearColor(0.34f, 0.13f, 0.02f, 0.82f * Pulse), BarX, 88.f, BarW, 52.f);
				DrawRect(FLinearColor(1.f, 0.55f, 0.10f, 0.95f), BarX, 88.f, BarW, 3.f);

				DrawText(FString::Printf(TEXT("冲撞锁定    %.1f s"), Remaining),
					FLinearColor(1.f, 0.93f, 0.85f, 1.f), BarX + 20.f, 96.f, nullptr, 2.2f);
				DrawText(TEXT("跑到碎石柱后面，让它撞上柱子（撞 3 根直接破防）"),
					FLinearColor(1.f, 0.78f, 0.42f, 1.f), BarX + 20.f, 152.f, nullptr, 1.3f);
			}
		}

		// ── 技能解锁提示（07 §3.3-C）：屏幕中央，1.2s ──
		if (const AChuandiBossCharacter* CueBoss = Cast<AChuandiBossCharacter>(Target))
		{
			if (CueBoss->IsUnlockCueActive())
			{
				const float BoxW = 520.f;
				const float BoxX = ScreenW * 0.5f - BoxW * 0.5f;
				const float BoxY = ScreenH * 0.34f;

				DrawRect(FLinearColor(0.10f, 0.08f, 0.02f, 0.75f), BoxX, BoxY, BoxW, 54.f);
				DrawRect(FLinearColor(1.f, 0.83f, 0.28f, 0.95f), BoxX, BoxY, BoxW, 3.f);
				DrawText(CueBoss->GetUnlockCueText(), FLinearColor(1.f, 0.90f, 0.55f, 1.f),
					BoxX + 26.f, BoxY + 12.f, nullptr, 2.0f);
			}

		}

		// Boss 状态机 + 怀疑度（V5/V6 取证用）
		if (const AChuandiBossCharacter* BossChar = Cast<AChuandiBossCharacter>(Target))
		{
			// 破防条：07 §1.8 UI 定稿（方案 B）—— 一条连续纯金色条，无分区
			// 底槽灰蓝（待积累）+ 金色填充（机会在积累），满 100% 整条闪烁
			if (const UBreakMeterComponent* Meter = BossChar->GetBreakMeter())
			{
				const float Ratio = Meter->GetNormalized();
				const bool bFull = Ratio >= 0.999f;

				const float BarW = 600.f;
				const float BarH = 6.f;
				const float BarX = ScreenW * 0.5f - BarW * 0.5f;
				const float BarY = 16.f;

				DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), BarX - 2.f, BarY - 2.f, BarW + 4.f, BarH + 4.f);
				DrawRect(FLinearColor(0.20f, 0.27f, 0.35f, 0.95f), BarX, BarY, BarW, BarH);

				// 满格闪烁：2Hz
				const float Blink = bFull
					? (0.55f + 0.45f * FMath::Abs(FMath::Sin(Now * PI * 2.f)))
					: 1.f;
				DrawRect(FLinearColor(1.f, 0.83f, 0.28f, 0.95f * Blink), BarX, BarY, BarW * Ratio, BarH);
			}

			// 状态机名 + 怀疑度条 = 开发信息，录 demo 按 F1 关掉（否则画面像 Dev Build）
			const AChuandiPlayerCharacter* DebugPlayer = Cast<AChuandiPlayerCharacter>(PlayerPawn);
			const bool bShowDebug = DebugPlayer && DebugPlayer->IsDebugHUDVisible();

			if (bShowDebug)
			{
				if (const UBossStateMachineComponent* FSM = BossChar->GetStateMachine())
				{
					const FString StateName = UEnum::GetDisplayValueAsText(FSM->GetState()).ToString();
					DrawText(StateName, FLinearColor(0.85f, 0.85f, 0.85f, 1.f),
						ScreenW * 0.5f - 300.f, 74.f, nullptr, 1.3f);
				}

				if (const UBossPerceptionComponent* Perception = BossChar->GetPerception())
				{
					const float Suspicion = Perception->GetSuspicion();
					const float BarW = 120.f;
					DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), ScreenW * 0.5f - 300.f, 100.f, BarW + 2.f, 8.f);
					DrawRect(Perception->HasLineOfSight()
						? FLinearColor(1.f, 0.6f, 0.1f, 0.95f)
						: FLinearColor(0.4f, 0.4f, 0.4f, 0.8f),
						ScreenW * 0.5f - 299.f, 101.f, BarW * Suspicion, 6.f);
				}
			}
		}
	}

	if (ParryFlashRemaining > 0.f)
	{
		ParryFlashRemaining -= DeltaSeconds;
		DrawText(TEXT("PARRY!"), ParryCyan, ScreenW * 0.5f - 62.f, ScreenH * 0.36f, nullptr, 3.2f);
	}

	if (PhaseAnnounceRemaining > 0.f)
	{
		PhaseAnnounceRemaining -= DeltaSeconds;
		const float Alpha = FMath::Clamp(PhaseAnnounceRemaining / 1.8f, 0.f, 1.f);
		DrawText(PhaseAnnounceText, FLinearColor(1.f, 0.83f, 0.28f, Alpha),
			ScreenW * 0.5f - 108.f, ScreenH * 0.26f, nullptr, 2.4f);
	}

	// ── 弹反成功/失败反馈（04 §4 动效优先级第 1 条）──
	if (const UChuandiAbilitySystemComponent* ChuandiASC =
		Cast<UChuandiAbilitySystemComponent>(PlayerASC))
	{
		// 成功：全屏白闪 0.1s（07 §1.4 白色闪光）
		const double SinceSuccess = Now - ChuandiASC->GetLastParrySuccessTime();
		if (SinceSuccess >= 0.0 && SinceSuccess < 0.1)
		{
			const float Alpha = 0.5f * static_cast<float>(1.0 - SinceSuccess / 0.1);
			DrawRect(FLinearColor(1.f, 1.f, 1.f, Alpha), 0.f, 0.f, ScreenW, ScreenH);
		}

		// 失败：屏幕暗角 0.3s（04 §4）
		const double SinceFail = Now - ChuandiASC->GetLastParryFailTime();
		if (SinceFail >= 0.0 && SinceFail < 0.3)
		{
			const float Alpha = 0.6f * static_cast<float>(1.0 - SinceFail / 0.3);
			const float Edge = 110.f;
			DrawRect(FLinearColor(0.f, 0.f, 0.f, Alpha), 0.f, 0.f, ScreenW, Edge);
			DrawRect(FLinearColor(0.f, 0.f, 0.f, Alpha), 0.f, ScreenH - Edge, ScreenW, Edge);
			DrawRect(FLinearColor(0.f, 0.f, 0.f, Alpha), 0.f, 0.f, Edge, ScreenH);
			DrawRect(FLinearColor(0.f, 0.f, 0.f, Alpha), ScreenW - Edge, 0.f, Edge, ScreenH);
		}
	}

	// ── 结算画面（10_玩法扩展方案 §1：把校验点变成玩家可见数据）──
	if (GM->IsVictory() || GM->IsDefeat())
	{
		const float PanelW = 420.f;
		const float PanelH = 300.f;
		const float PX = ScreenW * 0.5f - PanelW * 0.5f;
		const float PY = ScreenH * 0.5f - PanelH * 0.5f;

		DrawRect(FLinearColor(0.05f, 0.05f, 0.04f, 0.88f), PX, PY, PanelW, PanelH);
		DrawRect(FLinearColor(0.78f, 0.62f, 0.09f, 0.9f), PX, PY, PanelW, 2.f);

		const FLinearColor Gold(1.f, 0.83f, 0.28f, 1.f);
		const FLinearColor Gray(0.72f, 0.72f, 0.68f, 1.f);

		DrawText(GM->IsVictory() ? TEXT("VICTORY") : TEXT("DEFEAT"),
			GM->IsVictory() ? Gold : DangerRed, PX + 26.f, PY + 18.f, nullptr, 2.2f);

		// 评级：右下角大字
		DrawText(FString::Printf(TEXT("%s"), *GM->GetRank()), Gold,
			PX + PanelW - 96.f, PY + 12.f, nullptr, 4.2f);

		float LineY = PY + 84.f;
		auto Row = [&](const FString& Label, const FString& Value)
		{
			DrawText(Label, Gray, PX + 28.f, LineY, nullptr, 1.15f);
			DrawText(Value, FLinearColor::White, PX + 210.f, LineY, nullptr, 1.15f);
			LineY += 34.f;
		};

		Row(TEXT("用时"), FString::Printf(TEXT("%.1f s"), GM->GetElapsedSeconds()));

		const float Rate = GM->GetParryRate();
		Row(TEXT("弹反"), FString::Printf(TEXT("%d / %d  (%.0f%%)"),
			GM->GetParrySuccesses(), GM->GetParryAttempts(), Rate < 0.f ? 0.f : Rate * 100.f));

		Row(TEXT("破防次数"), FString::Printf(TEXT("%d"), GM->GetBreakCount()));
		Row(TEXT("承受伤害"), FString::Printf(TEXT("%.0f"), GM->GetDamageTaken()));

		DrawText(TEXT("按 R 重开"), Gray, PX + 28.f, PY + PanelH - 34.f, nullptr, 1.0f);
	}
}

namespace
{
	// 09_UI 规范：不画拟物图标，只靠形状 + 颜色
	const FLinearColor SlotIdle(0.13f, 0.52f, 0.62f, 0.9f);
	const FLinearColor SlotEmpty(0.12f, 0.12f, 0.11f, 0.85f);
	const FLinearColor SlotCooling(0.18f, 0.18f, 0.17f, 0.8f);
	const FLinearColor LabelNormal(0.54f, 0.54f, 0.51f, 1.f);   // #8A8A82
	const FLinearColor LabelDim(0.35f, 0.35f, 0.32f, 0.6f);      // #5A5A52 60%
	const FLinearColor UIGold(1.f, 0.83f, 0.28f, 1.f);            // #FFD447
}

void AChuandiHUD::DrawDisc(float CX, float CY, float Radius, const FLinearColor& Color, int32 Segments)
{
	if (!Canvas || Radius <= 0.f)
	{
		return;
	}

	TArray<FCanvasUVTri> Tris;
	Tris.Reserve(Segments);
	for (int32 i = 0; i < Segments; ++i)
	{
		const float A0 = 2.f * PI * i / Segments;
		const float A1 = 2.f * PI * (i + 1) / Segments;

		FCanvasUVTri Tri;
		Tri.V0_Pos = FVector2D(CX, CY);
		Tri.V1_Pos = FVector2D(CX + FMath::Cos(A0) * Radius, CY + FMath::Sin(A0) * Radius);
		Tri.V2_Pos = FVector2D(CX + FMath::Cos(A1) * Radius, CY + FMath::Sin(A1) * Radius);
		Tri.V0_UV = Tri.V1_UV = Tri.V2_UV = FVector2D::ZeroVector;
		Tri.V0_Color = Tri.V1_Color = Tri.V2_Color = Color;
		Tris.Add(Tri);
	}

	FCanvasTriangleItem Item(Tris, GWhiteTexture);
	Canvas->DrawItem(Item);
}

void AChuandiHUD::DrawRing(float CX, float CY, float Radius, float Thickness, const FLinearColor& Color,
	int32 Segments)
{
	if (!Canvas || Radius <= 0.f)
	{
		return;
	}

	const float Inner = FMath::Max(0.f, Radius - Thickness);
	TArray<FCanvasUVTri> Tris;
	Tris.Reserve(Segments * 2);

	for (int32 i = 0; i < Segments; ++i)
	{
		const float A0 = 2.f * PI * i / Segments;
		const float A1 = 2.f * PI * (i + 1) / Segments;

		const FVector2D O0(CX + FMath::Cos(A0) * Radius, CY + FMath::Sin(A0) * Radius);
		const FVector2D O1(CX + FMath::Cos(A1) * Radius, CY + FMath::Sin(A1) * Radius);
		const FVector2D I0(CX + FMath::Cos(A0) * Inner, CY + FMath::Sin(A0) * Inner);
		const FVector2D I1(CX + FMath::Cos(A1) * Inner, CY + FMath::Sin(A1) * Inner);

		FCanvasUVTri T0;
		T0.V0_Pos = I0; T0.V1_Pos = O0; T0.V2_Pos = O1;
		T0.V0_UV = T0.V1_UV = T0.V2_UV = FVector2D::ZeroVector;
		T0.V0_Color = T0.V1_Color = T0.V2_Color = Color;
		Tris.Add(T0);

		FCanvasUVTri T1;
		T1.V0_Pos = I0; T1.V1_Pos = O1; T1.V2_Pos = I1;
		T1.V0_UV = T1.V1_UV = T1.V2_UV = FVector2D::ZeroVector;
		T1.V0_Color = T1.V1_Color = T1.V2_Color = Color;
		Tris.Add(T1);
	}

	FCanvasTriangleItem Item(Tris, GWhiteTexture);
	Canvas->DrawItem(Item);
}

void AChuandiHUD::DrawSector(float CX, float CY, float Radius, float StartDeg, float EndDeg,
	const FLinearColor& Color, int32 Segments)
{
	if (!Canvas || Radius <= 0.f || EndDeg <= StartDeg)
	{
		return;
	}

	TArray<FCanvasUVTri> Tris;
	Tris.Reserve(Segments);
	const float Step = (EndDeg - StartDeg) / Segments;
	for (int32 i = 0; i < Segments; ++i)
	{
		const float A0 = FMath::DegreesToRadians(StartDeg + Step * i);
		const float A1 = FMath::DegreesToRadians(StartDeg + Step * (i + 1));

		FCanvasUVTri Tri;
		Tri.V0_Pos = FVector2D(CX, CY);
		Tri.V1_Pos = FVector2D(CX + FMath::Cos(A0) * Radius, CY + FMath::Sin(A0) * Radius);
		Tri.V2_Pos = FVector2D(CX + FMath::Cos(A1) * Radius, CY + FMath::Sin(A1) * Radius);
		Tri.V0_UV = Tri.V1_UV = Tri.V2_UV = FVector2D::ZeroVector;
		Tri.V0_Color = Tri.V1_Color = Tri.V2_Color = Color;
		Tris.Add(Tri);
	}

	FCanvasTriangleItem Item(Tris, GWhiteTexture);
	Canvas->DrawItem(Item);
}

void AChuandiHUD::DrawDiamond(float CX, float CY, float Size, const FLinearColor& Color)
{
	if (!Canvas || Size <= 0.f)
	{
		return;
	}

	const FVector2D Top(CX, CY - Size * 0.5f);
	const FVector2D Right(CX + Size * 0.5f, CY);
	const FVector2D Bottom(CX, CY + Size * 0.5f);
	const FVector2D Left(CX - Size * 0.5f, CY);
	const FVector2D Center(CX, CY);

	const FVector2D Pts[4] = { Top, Right, Bottom, Left };
	TArray<FCanvasUVTri> Tris;
	Tris.Reserve(4);
	for (int32 i = 0; i < 4; ++i)
	{
		FCanvasUVTri Tri;
		Tri.V0_Pos = Center;
		Tri.V1_Pos = Pts[i];
		Tri.V2_Pos = Pts[(i + 1) % 4];
		Tri.V0_UV = Tri.V1_UV = Tri.V2_UV = FVector2D::ZeroVector;
		Tri.V0_Color = Tri.V1_Color = Tri.V2_Color = Color;
		Tris.Add(Tri);
	}

	FCanvasTriangleItem Item(Tris, GWhiteTexture);
	Canvas->DrawItem(Item);
}

void AChuandiHUD::DrawSlotLabel(float CX, float Y, const FString& Text, bool bDim)
{
	// AHUD::DrawText 不居中，按字符数近似回退半个宽度
	const float ApproxWidth = Text.Len() * 9.f;
	DrawText(Text, bDim ? LabelDim : LabelNormal, CX - ApproxWidth * 0.5f, Y, nullptr, 0.9f);
}

void AChuandiHUD::DrawSkillBar(AActor* PlayerPawn, float ScreenW, float ScreenH, double Now)
{
	UAbilitySystemComponent* PlayerASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerPawn);
	if (!PlayerASC)
	{
		return;
	}

	const AChuandiPlayerCharacter* PlayerChar = Cast<AChuandiPlayerCharacter>(PlayerPawn);
	const bool bParryCooling = PlayerASC->HasMatchingGameplayTag(ChuandiTags::Cooldown_Parry.GetTag());
	const bool bDodgeCooling = PlayerASC->HasMatchingGameplayTag(ChuandiTags::Cooldown_Dodge.GetTag());
	const bool bSkillCooling = PlayerASC->HasMatchingGameplayTag(ChuandiTags::Cooldown_Skill.GetTag());
	const bool bParryWindow = PlayerASC->HasMatchingGameplayTag(ChuandiTags::State_Parrying.GetTag());
	const bool bAirborne = PlayerChar && PlayerChar->GetCharacterMovement()
		&& PlayerChar->GetCharacterMovement()->IsFalling();
	const int32 TrapCharges = PlayerChar ? PlayerChar->GetTrapCharges() : 0;

	// 坐标来自 CHANGELOG 16:04（距右下角 px）
	auto CenterOf = [&](float RightPx, float BottomPx, float Diameter)
	{
		return FVector2D(ScreenW - RightPx - Diameter * 0.5f, ScreenH - BottomPx - Diameter * 0.5f);
	};

	// ① 攻击：64px 实心圆，永远常亮（无冷却无消耗）
	{
		const float D = 64.f;
		const FVector2D C = CenterOf(56.f, 56.f, D);
		DrawDisc(C.X, C.Y, D * 0.5f, FLinearColor(0.86f, 0.86f, 0.84f, 0.95f));
		DrawSlotLabel(C.X, C.Y + D * 0.5f + 6.f, TEXT("左键"), false);
	}

	// ② 技能 1（伤害技能，1 键）：48px
	{
		const float D = 48.f;
		const FVector2D C = CenterOf(148.f, 56.f, D);
		DrawDisc(C.X, C.Y, D * 0.5f, SlotIdle);

		// 冷却扫回：从 12 点方向顺时针扫掉，剩余部分灰化
		const float SkillRatio = PlayerChar ? PlayerChar->GetSkillCooldownRatio() : 0.f;
		if (SkillRatio > 0.f)
		{
			const float StartDeg = -90.f + (1.f - SkillRatio) * 360.f;
			DrawSector(C.X, C.Y, D * 0.5f, StartDeg, 270.f, FLinearColor(0.08f, 0.08f, 0.08f, 0.75f));
		}
		DrawSlotLabel(C.X, C.Y + D * 0.5f + 6.f, TEXT("1"), false);
	}

	// ③ 技能 2（隔空布阱，2 键）：槽内 3 个资源点，用完整槽灰
	{
		const float D = 48.f;
		const FVector2D C = CenterOf(204.f, 56.f, D);
		const bool bExhausted = TrapCharges <= 0;
		DrawDisc(C.X, C.Y, D * 0.5f, bExhausted ? SlotCooling : SlotIdle);

		for (int32 i = 0; i < 3; ++i)
		{
			const float DotX = C.X - 10.f + i * 10.f;
			DrawDisc(DotX, C.Y, 3.2f,
				i < TrapCharges ? FLinearColor(0.83f, 0.63f, 0.09f, 1.f)
				: FLinearColor(0.25f, 0.25f, 0.23f, 0.8f));
		}
		DrawSlotLabel(C.X, C.Y + D * 0.5f + 6.f, TEXT("2"), false);
	}

	// ④ 技能 3 / 4：空槽（只有暗灰底圆，看起来像"预留位"）
	{
		const float D = 48.f;
		const float Rights[2] = { 260.f, 316.f };
		const TCHAR* Names[2] = { TEXT("3"), TEXT("4") };
		for (int32 i = 0; i < 2; ++i)
		{
			const FVector2D C = CenterOf(Rights[i], 56.f, D);
			DrawDisc(C.X, C.Y, D * 0.5f, SlotEmpty);
			// 09 §3.1：空槽里写灰字"未解锁"——让评委看到四格是规划，不是做不出来
			DrawText(TEXT("未解锁"), LabelDim, C.X - 14.f, C.Y - 6.f, nullptr, 0.85f);
			DrawSlotLabel(C.X, C.Y + D * 0.5f + 6.f, Names[i], true);
		}
	}

	// ⑤ 弹反：40px 菱形，三态（细金环 / 4Hz 脉冲粗环 / 冷却灰化）
	{
		const float D = 40.f;
		const FVector2D C = CenterOf(148.f, 120.f, D);

		FLinearColor Fill = FLinearColor(0.10f, 0.10f, 0.09f, 0.9f);
		FLinearColor RingColor = UIGold;
		float RingThickness = 1.5f;

		if (bParryWindow)
		{
			// 窗口期：粗金环 + 4Hz 脉冲
			const float Pulse = 0.55f + 0.45f * FMath::Abs(FMath::Sin(Now * PI * 8.f));
			RingColor = UIGold * Pulse;
			RingThickness = 3.f;
		}
		else if (bParryCooling)
		{
			// 灰化扫回：越接近就绪越亮（菱形不好画扇形，用亮度渐变代替）
			const float ParryRatio = PlayerChar ? PlayerChar->GetParryCooldownRatio() : 0.f;
			const float Bright = FMath::Lerp(0.85f, 0.25f, ParryRatio);
			RingColor = FLinearColor(Bright, Bright * 0.97f, Bright * 0.75f, 0.85f);
			RingThickness = 1.5f;
			Fill = FLinearColor(0.10f, 0.10f, 0.09f, 0.9f - (1.f - ParryRatio) * 0.25f);
		}

		DrawDiamond(C.X, C.Y, D, Fill);
		DrawDiamond(C.X, C.Y, D, RingColor);
		DrawDiamond(C.X, C.Y, D - RingThickness * 2.f, Fill);
		DrawSlotLabel(C.X, C.Y + D * 0.5f + 6.f, TEXT("Q"), false);
	}

	// ⑥ 跳跃（空格，空心方框，常亮）
	{
		const float D = 40.f;
		const FVector2D C = CenterOf(56.f, 136.f, D);
		const FLinearColor C1(0.70f, 0.70f, 0.67f, 0.95f);
		DrawRect(C1, C.X - D * 0.5f, C.Y - D * 0.5f, D, 2.f);
		DrawRect(C1, C.X - D * 0.5f, C.Y + D * 0.5f - 2.f, D, 2.f);
		DrawRect(C1, C.X - D * 0.5f, C.Y - D * 0.5f, 2.f, D);
		DrawRect(C1, C.X + D * 0.5f - 2.f, C.Y - D * 0.5f, 2.f, D);
		DrawSlotLabel(C.X, C.Y + D * 0.5f + 6.f, TEXT("空格"), false);
	}

	// ⑦ 闪避（Shift，实心方块）：冷却中或滞空时变灰
	{
		const float D = 40.f;
		const FVector2D C = CenterOf(112.f, 136.f, D);
		const bool bDisabled = bDodgeCooling || bAirborne;
		DrawRect(FLinearColor(0.13f, 0.52f, 0.62f, 0.9f), C.X - D * 0.5f, C.Y - D * 0.5f, D, D);

		// 冷却扫回：方块从下往上恢复（剩余比例 = 灰色高度）
		const float DodgeRatio = PlayerChar ? PlayerChar->GetDodgeCooldownRatio() : 0.f;
		if (DodgeRatio > 0.f)
		{
			const float MaskH = D * DodgeRatio;
			DrawRect(FLinearColor(0.08f, 0.08f, 0.08f, 0.8f),
				C.X - D * 0.5f, C.Y - D * 0.5f, D, MaskH);
		}
		if (bAirborne)
		{
			DrawRect(FLinearColor(0.08f, 0.08f, 0.08f, 0.55f), C.X - D * 0.5f, C.Y - D * 0.5f, D, D);
		}
		DrawSlotLabel(C.X, C.Y + D * 0.5f + 6.f, TEXT("Shift"), bDisabled);
	}
}

void AChuandiHUD::DrawHealthBarFor(AActor* Actor, float X, float Y, float Width, float Height,
	const FLinearColor& Fill, const FString& Label)
{
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	const UChuandiAttributeSet* Attr = ASC ? ASC->GetSet<UChuandiAttributeSet>() : nullptr;
	if (!Attr)
	{
		return;
	}

	const float Current = Attr->GetHealth();
	const float Max = FMath::Max(1.f, Attr->GetMaxHealth());
	const float Ratio = FMath::Clamp(Current / Max, 0.f, 1.f);

	// 阴影层
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.50f), X - 4.f, Y - 4.f, Width + 8.f, Height + 8.f);

	// 轨道
	DrawRect(FLinearColor(0.07f, 0.07f, 0.08f, 0.92f), X, Y, Width, Height);

	// 填充 + 顶部高光（让平涂方块有一点体积感）
	DrawRect(Fill, X, Y, Width * Ratio, Height);
	DrawRect(FLinearColor(
		FMath::Min(1.f, Fill.R + 0.14f),
		FMath::Min(1.f, Fill.G + 0.14f),
		FMath::Min(1.f, Fill.B + 0.14f), 0.35f),
		X, Y, Width * Ratio, Height * 0.34f);

	// 25% 刻度：扣血量变成"看得见的几段"
	for (int32 i = 1; i < 4; ++i)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), X + Width * (i * 0.25f), Y, 1.f, Height);
	}

	// 1px 描边
	const FLinearColor Edge(0.78f, 0.78f, 0.78f, 0.22f);
	DrawRect(Edge, X, Y, Width, 1.f);
	DrawRect(Edge, X, Y + Height - 1.f, Width, 1.f);
	DrawRect(Edge, X, Y, 1.f, Height);
	DrawRect(Edge, X + Width - 1.f, Y, 1.f, Height);

	// 名字在条内左侧，数值右对齐（原来挤在条上方，和破防条抢位置）
	DrawText(Label, FLinearColor(1.f, 1.f, 1.f, 0.86f), X + 10.f, Y + (Height - 14.f) * 0.5f, nullptr, 1.0f);

	const FString ValueText = FString::Printf(TEXT("%.0f / %.0f"), Current, Max);
	float TextW = 0.f;
	float TextH = 0.f;
	GetTextSize(ValueText, TextW, TextH, nullptr, 1.0f);
	DrawText(ValueText, FLinearColor(1.f, 1.f, 1.f, 0.95f),
		X + Width - TextW - 10.f, Y + (Height - TextH) * 0.5f, nullptr, 1.0f);
}
