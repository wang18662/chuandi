#include "L3_Systems/GAS/ChuandiCombatLibrary.h"

#include "L0_Core/ChuandiLog.h"
#include "L0_Core/ChuandiNativeTags.h"
#include "L1_Framework/ChuandiGameMode.h"
#include "L2_Entities/Boss/ChuandiBossCharacter.h"
#include "L3_Systems/BossStateMachineComponent.h"
#include "L3_Systems/GAS/ChuandiBreakEffect.h"
#include "L3_Systems/GAS/ChuandiDamageEffect.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

float UChuandiCombatLibrary::ApplyDamage(AActor* Source, AActor* Target, float Damage)
{
	if (Damage <= 0.f)
	{
		return 0.f;
	}

	// 已死亡的目标不再吃伤害（否则尸体会被继续打，日志也会刷屏）
	if (HasTag(Target, ChuandiTags::State_Dead.GetTag()))
	{
		return 0.f;
	}

	// 闪避无敌帧（07 §0）
	if (HasTag(Target, ChuandiTags::State_Invincible.GetTag()))
	{
		UE_LOG(LogChuandi, Log, TEXT("[Combat] %s 处于无敌帧，伤害丢弃"), *GetNameSafe(Target));
		return 0.f;
	}

	// 遁地 = 不可选中（04 §2.1：半透明，打不到）
	if (HasTag(Target, ChuandiTags::State_Burrowed.GetTag()))
	{
		UE_LOG(LogChuandi, Log, TEXT("[Combat] %s 正在遁地，不可选中"), *GetNameSafe(Target));
		return 0.f;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!TargetASC)
	{
		UE_LOG(LogChuandi, Warning, TEXT("[Combat] %s 身上没有 ASC，伤害丢弃"), *GetNameSafe(Target));
		return 0.f;
	}

	UAbilitySystemComponent* SourceASC = Source
		? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Source, /*LookForComponent=*/false)
		: nullptr;
	if (!SourceASC)
	{
		SourceASC = TargetASC;
	}

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddSourceObject(Source);

	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(
		UChuandiDamageEffect::StaticClass(), 1.f, Context);

	if (!Spec.IsValid())
	{
		UE_LOG(LogChuandi, Error, TEXT("[Combat] MakeOutgoingSpec 失败，检查 UChuandiDamageEffect 是否合法"));
		return 0.f;
	}

	// 伤害值走 SetByCaller（负值 = 扣血）。不能依赖 GE Level，见 ChuandiDamageEffect.h 的注释
	Spec.Data->SetSetByCallerMagnitude(ChuandiTags::Data_Damage.GetTag(), -Damage);

	SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);

	// 受击唤醒：Boss 被打就该知道谁打的，否则从背后站桩偷袭可以无伤刷血。
	// 注意 Broken / Frozen 期间不唤醒——被控时挨打是应该的，否则破防毫无意义。
	if (AChuandiBossCharacter* Boss = Cast<AChuandiBossCharacter>(Target))
	{
		if (!HasTag(Target, ChuandiTags::State_Broken.GetTag())
			&& !HasTag(Target, ChuandiTags::State_Frozen.GetTag()))
		{
			if (UBossStateMachineComponent* FSM = Boss->GetStateMachine())
			{
				FSM->NotifyDamaged(Source);
			}
		}
	}

	// 结算统计：玩家承受的伤害（结算面板显示）
	if (Target && Target == UGameplayStatics::GetPlayerPawn(Target, 0))
	{
		if (AChuandiGameMode* GM = Cast<AChuandiGameMode>(UGameplayStatics::GetGameMode(Target)))
		{
			GM->RegisterDamageTaken(Damage);
		}
	}

	UE_LOG(LogChuandi, Verbose, TEXT("[Combat] %s → %s  伤害 %.1f"),
		*GetNameSafe(Source), *GetNameSafe(Target), Damage);

	return Damage;
}

bool UChuandiCombatLibrary::ApplyStatusGE(AActor* Source, AActor* Target, TSubclassOf<UGameplayEffect> EffectClass, float Level)
{
	if (!EffectClass)
	{
		return false;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!TargetASC)
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = Source
		? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Source, false)
		: nullptr;
	if (!SourceASC)
	{
		SourceASC = TargetASC;
	}

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddSourceObject(Source);

	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(EffectClass, Level, Context);
	if (!Spec.IsValid())
	{
		return false;
	}

	SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	return true;
}

bool UChuandiCombatLibrary::HasTag(const AActor* Target, FGameplayTag Tag)
{
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	return ASC && ASC->HasMatchingGameplayTag(Tag);
}

bool UChuandiCombatLibrary::ApplyBreak(AActor* Source, AActor* Target)
{
	if (!Target)
	{
		return false;
	}

	// 破防 GE（2s）+ 把 State.Broken 挂在 spec 上
	const bool bApplied = ApplyStatusGEWithTags(Source, Target, UChuandiBreakEffect::StaticClass(),
		ChuandiTags::State_Broken.GetTag());

	if (!HasTag(Target, ChuandiTags::State_Broken.GetTag()))
	{
		UE_LOG(LogChuandi, Error, TEXT("[Parry] 破防 tag 依然没挂上（GE 应用=%d），请检查 tag 注册"),
			bApplied ? 1 : 0);
		return false;
	}

	UE_LOG(LogChuandi, Log, TEXT("[Parry] 弹反成功 → %s 进入破防（2s，受伤 x2）"), *GetNameSafe(Target));
	return true;
}

void UChuandiCombatLibrary::GlobalSlowMo(UWorld* World, float RealDuration, float Dilation)
{
	if (!World || RealDuration <= 0.f || Dilation <= 0.f || FMath::IsNearlyEqual(Dilation, 1.f))
	{
		return;
	}

	const float Previous = UGameplayStatics::GetGlobalTimeDilation(World);
	UGameplayStatics::SetGlobalTimeDilation(World, Dilation);

	// 用真实时间恢复：世界被减速时 timer 也跟着变慢，所以这里挂在不受 dilation 影响的地方
	FTimerHandle Handle;
	FTimerDelegate Delegate = FTimerDelegate::CreateLambda([World, Previous]()
	{
		if (IsValid(World))
		{
			UGameplayStatics::SetGlobalTimeDilation(World, Previous);
		}
	});

	// 恢复用的 timer 走游戏时间：世界被压到 0.05 倍后，游戏时间只走现实时间的 0.05
	// → 要等 RealDuration 现实秒，timer 只需设 RealDuration * Dilation 游戏秒
	World->GetTimerManager().SetTimer(Handle, Delegate, RealDuration * Dilation, false);
}

bool UChuandiCombatLibrary::ApplyStatusGEWithTags(AActor* Source, AActor* Target,
	TSubclassOf<UGameplayEffect> EffectClass, FGameplayTag GrantedTag, float Level, float DurationOverride)
{
	if (!EffectClass)
	{
		return false;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!TargetASC)
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = Source
		? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Source, false)
		: nullptr;
	if (!SourceASC)
	{
		SourceASC = TargetASC;
	}

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddSourceObject(Source);

	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(EffectClass, Level, Context);
	if (!Spec.IsValid())
	{
		return false;
	}

	// ★ 运行时挂 tag（不要写在 GE 的构造函数里，那个时机原生 tag 还没注册）
	if (GrantedTag.IsValid())
	{
		Spec.Data->DynamicGrantedTags.AddTag(GrantedTag);
	}

	// 时长可由调用方覆盖（让 TrapFreezeDuration 这类数值真正生效，而不是躺在资产里）
	if (DurationOverride > 0.f)
	{
		Spec.Data->SetDuration(DurationOverride, /*bLockDuration=*/true);
	}

	SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	return true;
}

void UChuandiCombatLibrary::HitStop(UWorld* World, float RealSeconds, float Dilation)
{
	if (!World || RealSeconds <= 0.f)
	{
		return;
	}

	UGameplayStatics::SetGlobalTimeDilation(World, FMath::Max(0.01f, Dilation));

	TWeakObjectPtr<UWorld> WeakWorld(World);
	TSharedPtr<double> Remaining = MakeShared<double>(static_cast<double>(RealSeconds));

	// FTSTicker 用的是真实时间，不受 SetGlobalTimeDilation 影响 → 能正确恢复（R3）
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[WeakWorld, Remaining](float DeltaRealSeconds) -> bool
		{
			*Remaining -= DeltaRealSeconds;
			if (*Remaining <= 0.0)
			{
				if (UWorld* ValidWorld = WeakWorld.Get())
				{
					UGameplayStatics::SetGlobalTimeDilation(ValidWorld, 1.f);
				}
				return false; // 结束 ticker
			}
			return true;
		}));
}
