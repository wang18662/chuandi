#include "L3_Systems/GAS/ChuandiSlowEffect.h"

#include "L3_Systems/GAS/ChuandiAttributeSet.h"

UChuandiSlowEffect::UChuandiSlowEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	// D44：减速持续 2 秒
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(2.f));

	FGameplayModifierInfo Info;
	Info.Attribute = UChuandiAttributeSet::GetMoveSpeedAttribute();
	Info.ModifierOp = EGameplayModOp::Additive;
	// 500 → 250，速度减半；常数不需要 Level 缩放，所以这里用 ScalableFloat 是安全的
	Info.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-250.f));

	Modifiers.Add(Info);
}
