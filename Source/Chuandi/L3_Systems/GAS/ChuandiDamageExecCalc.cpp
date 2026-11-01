#include "L3_Systems/GAS/ChuandiDamageExecCalc.h"

#include "L0_Core/ChuandiLog.h"
#include "L0_Core/ChuandiNativeTags.h"
#include "L1_Framework/ChuandiTuningData.h"
#include "L3_Systems/GAS/ChuandiAttributeSet.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"

UChuandiDamageExecCalc::UChuandiDamageExecCalc()
{
	// 这个计算不需要捕获任何属性（伤害完全由 SetByCaller 与 tag 决定）
	// 如果以后要算"按目标当前血量比例"的伤害，在这里加 Health 的捕获
}

void UChuandiDamageExecCalc::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	float Damage = Spec.GetSetByCallerMagnitude(ChuandiTags::Data_Damage.GetTag(), /*bWarnIfNotFound=*/false, 0.f);
	if (Damage == 0.f)
	{
		return;
	}

	const UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
	if (!TargetASC)
	{
		return;
	}

	// ── 倍率规则：按"应当先算哪个"的顺序写，别依赖 GE 的应用顺序 ──
	float Multiplier = 1.f;

	if (TargetASC->HasMatchingGameplayTag(ChuandiTags::State_Broken.GetTag()))
	{
		// 07：破防 ×15（4 → 60）。倍率放 TuningData，别写死在代码里
		const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
		Multiplier *= Tuning ? Tuning->BreakDamageMultiplier : 15.f;
	}
	if (TargetASC->HasMatchingGameplayTag(ChuandiTags::State_Vulnerable.GetTag()))
	{
		Multiplier *= 1.5f;  // 预留：易伤陷阱
	}

	const float FinalDamage = Damage * Multiplier;

	FGameplayTagContainer TargetTags;
	const_cast<UAbilitySystemComponent*>(TargetASC)->GetOwnedGameplayTags(TargetTags);
	UE_LOG(LogChuandi, Log, TEXT("[Damage] %.1f × %.2f = %.1f  → %s  tags=[%s]"),
		Damage, Multiplier, FinalDamage, *GetNameSafe(TargetASC->GetAvatarActor()),
		*TargetTags.ToStringSimple());

	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
		UChuandiAttributeSet::GetHealthAttribute(),
		EGameplayModOp::Additive,
		FinalDamage));
}
