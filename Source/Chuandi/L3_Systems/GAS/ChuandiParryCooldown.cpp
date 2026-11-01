#include "L3_Systems/GAS/ChuandiParryCooldown.h"

#include "L0_Core/ChuandiNativeTags.h"

UChuandiParryCooldown::UChuandiParryCooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	// D44：0.8 秒冷却
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.8f));

	// ★ 同上：Cooldown.Parry 不在 CDO 里授权，由 GA_PlayerParry 挂到 spec 上。
}
