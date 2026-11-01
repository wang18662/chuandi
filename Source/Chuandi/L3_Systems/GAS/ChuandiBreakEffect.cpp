#include "L3_Systems/GAS/ChuandiBreakEffect.h"

#include "L0_Core/ChuandiNativeTags.h"

UChuandiBreakEffect::UChuandiBreakEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	// D22：破防持续 2 秒（约 3 次普攻的窗口）
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(2.f));

	// ★ 不要在这里 AddTag：GE 的 CDO 构造早于原生 tag 注册，
	//   写进 InheritableOwnedTagsContainer 的是空 tag，运行时永远不生效。
	//   State.Broken 由 UChuandiCombatLibrary::ApplyStatusGEWithTags 挂到 spec 上。
}
