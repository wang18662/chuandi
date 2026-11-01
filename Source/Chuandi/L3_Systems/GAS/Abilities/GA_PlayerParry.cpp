#include "L3_Systems/GAS/Abilities/GA_PlayerParry.h"

#include "L0_Core/ChuandiLog.h"
#include "L0_Core/ChuandiNativeTags.h"
#include "L1_Framework/ChuandiGameMode.h"
#include "L1_Framework/ChuandiTuningData.h"
#include "L3_Systems/GAS/ChuandiAttributeSet.h"
#include "L2_Entities/Player/ChuandiPlayerCharacter.h"
#include "L3_Systems/GAS/ChuandiAbilitySystemComponent.h"
#include "L3_Systems/GAS/ChuandiCombatLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "L3_Systems/GAS/ChuandiParryCooldown.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"

UGA_PlayerParry::UGA_PlayerParry()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	// 不用 GAS 的 CooldownGameplayEffectClass：它靠 GE 类的 CDO tag 判断，
	// 而原生 tag 在 CDO 构造时还不可用（见 ChuandiCombatLibrary::ApplyStatusGEWithTags 的注释）。
	// 改为自己打 Cooldown.Parry tag + 查 tag。
}

void UGA_PlayerParry::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float Window = Tuning ? Tuning->ParryWindow : 0.35f;
	const float Cooldown = Tuning ? Tuning->ParryCooldown : 0.8f;

	// 弹反期间定身（10-28 20:21 用户要求）：开窗口就锁住移动，窗口结束再放开
	if (AChuandiPlayerCharacter* ParryChar = Cast<AChuandiPlayerCharacter>(Avatar))
	{
		ParryChar->BeginParryRoot();
	}

	// 受击硬直 / 后摇中按不出弹反（07 §1.2 进入条件）
	if (const AChuandiPlayerCharacter* PlayerChar = Cast<AChuandiPlayerCharacter>(Avatar))
	{
		if (PlayerChar->IsActionLocked())
		{
			UE_LOG(LogChuandi, Verbose, TEXT("[Parry] 硬直中，按不出"));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
	}

	// 手动冷却检查
	if (UChuandiCombatLibrary::HasTag(Avatar, ChuandiTags::Cooldown_Parry.GetTag()))
	{
		UE_LOG(LogChuandi, Verbose, TEXT("[Parry] 冷却中，按键无效"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UChuandiAbilitySystemComponent* ChuandiASC =
		Cast<UChuandiAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
	UWorld* World = GetWorld();

	if (!ChuandiASC || !World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	// 打冷却 tag（0.8s，挂在 spec 上所以可靠）
	UChuandiCombatLibrary::ApplyStatusGEWithTags(Avatar, Avatar,
		UChuandiParryCooldown::StaticClass(), ChuandiTags::Cooldown_Parry.GetTag(), 1.f, Cooldown);

	// 窗口起点与命中帧用同一个时间源（GetTimeSeconds），见 04 §5.1
	const double Now = World->GetTimeSeconds();
	ChuandiASC->OpenParryWindow(Now, Window);

	// 记录开窗时血量（用于窗口结束时区分"空放 / 失败"）
	HealthAtWindowStart = 0.f;
	if (const UChuandiAttributeSet* Attr = ChuandiASC->GetSet<UChuandiAttributeSet>())
	{
		HealthAtWindowStart = Attr->GetHealth();
	}

	// 同步加 tag：本帧内 Boss 的命中帧就能看到（走 GE 会晚一帧 → R9）
	ChuandiASC->AddLooseGameplayTag(ChuandiTags::State_Parrying.GetTag());

	UE_LOG(LogChuandi, Log, TEXT("[Parry] 开窗 %.3f → %.3f"), Now, Now + Window);

	UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, Window);
	DelayTask->OnFinish.AddDynamic(this, &UGA_PlayerParry::OnWindowEnd);
	DelayTask->ReadyForActivation();
}

void UGA_PlayerParry::OnWindowEnd()
{
	// ★ 设计意图（2026-10-25 用户明确）：**不做输入缓冲**。
	//   后摇 / 冷却期间按 Q 必须无效，不能"存起来等能动了再放"。
	//   弹反的代价就是这 0.1s 后摇——盖住它等于取消代价。
	//   所以这里只做锁定，不记录任何待执行的输入。
	//   （07 §1.1 那行"输入缓冲 120ms"与此冲突，以设计意图为准。）
	// 先取"这一窗有没有弹到"，再重置（重置会把标记清掉）
	bool bSucceeded = false;
	if (UChuandiAbilitySystemComponent* ChuandiASC =
		Cast<UChuandiAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
	{
		bSucceeded = ChuandiASC->WasParryConsumed();
		ChuandiASC->RemoveLooseGameplayTag(ChuandiTags::State_Parrying.GetTag());
		ChuandiASC->ResetParryWindow();
	}

	// 07 §1.2（11:25 版）区分三种结局：
	//   成功      → 后摇 0.1s
	//   空放      → 窗口自然结束，无硬直（只走 0.1s 后摇）
	//   失败      → 窗口内确实挨了打但没盖住命中帧 → 额外硬直 0.3s
	// 1.0s 窗口 + 1.2s 前摇下，"失败"已极罕见，多数情况是空放。
	if (AChuandiPlayerCharacter* Player = Cast<AChuandiPlayerCharacter>(GetAvatarActorFromActorInfo()))
	{
		const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
		const float Recovery = Tuning ? Tuning->ParryRecovery : 0.1f;
		const float FailStun = Tuning ? Tuning->ParryFailStun : 0.3f;

		float CurrentHealth = HealthAtWindowStart;
		if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			if (const UChuandiAttributeSet* Attr = ASC->GetSet<UChuandiAttributeSet>())
			{
				CurrentHealth = Attr->GetHealth();
			}
		}

		// 结算统计：成功一次
		if (bSucceeded)
		{
			if (AChuandiGameMode* GM = Cast<AChuandiGameMode>(UGameplayStatics::GetGameMode(this)))
			{
				GM->RegisterParrySuccess();
			}

			// 11 表现层 §2.2：「弹反成功那 0.15 秒是全场最值钱的一帧」
			// 现实 0.15s 顿帧（时间 dilation 0.05，结束后恢复），配一次镜头后推。
			UChuandiCombatLibrary::HitStop(GetWorld(), 0.15f, 0.05f);
			if (AActor* AvatarActor = GetAvatarActorFromActorInfo())
			{
				if (AChuandiPlayerCharacter* PC = Cast<AChuandiPlayerCharacter>(AvatarActor))
				{
					PC->PlayCameraKick(14.f);
				}
			}
		}

		const bool bTookDamage = CurrentHealth < HealthAtWindowStart - KINDA_SMALL_NUMBER;
		const bool bFailed = !bSucceeded && bTookDamage;
		const float Lock = bFailed ? FailStun : Recovery;

		Player->EndParryRoot();   // 解除定身，后摇由 LockActions 接管
		Player->LockActions(Lock);

		UE_LOG(LogChuandi, Log, TEXT("[Parry] %s → 锁定 %.2fs"),
			bSucceeded ? TEXT("成功") : (bFailed ? TEXT("失败（窗口内挨打）") : TEXT("空放")), Lock);
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
