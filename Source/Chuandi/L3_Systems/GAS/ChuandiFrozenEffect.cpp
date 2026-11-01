#include "L3_Systems/GAS/ChuandiFrozenEffect.h"

UChuandiFrozenEffect::UChuandiFrozenEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	// D22：陷阱效果持续 3 秒；D27：只控不破，所以这里不挂任何属性 modifier
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(3.f));

	// tag 在 ApplyStatusGEWithTags 里挂到 spec 上（CDO 时原生 tag 不可用）
}
