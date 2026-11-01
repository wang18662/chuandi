#include "L3_Systems/GAS/ChuandiAbilitySystemComponent.h"

#include "L0_Core/ChuandiCombatMath.h"
#include "L0_Core/ChuandiLog.h"

#include "Engine/World.h"

void UChuandiAbilitySystemComponent::OpenParryWindow(double Now, double Duration)
{
	ParryWindowStart = Now;
	ParryWindowEnd = Now + Duration;
	bParryConsumed = false;

	UE_LOG(LogChuandi, Verbose, TEXT("[Parry] 开窗 [%.3f, %.3f]"), ParryWindowStart, ParryWindowEnd);
}

void UChuandiAbilitySystemComponent::ConsumeParryWindow()
{
	bParryConsumed = true;

	if (const UWorld* World = GetWorld())
	{
		LastParrySuccessTime = World->GetTimeSeconds();
	}
}

void UChuandiAbilitySystemComponent::ResetParryWindow()
{
	// 收窗时还没被消耗 → 这一窗是空挥，HUD 用这个时间做"失败暗角"
	if (!bParryConsumed)
	{
		if (const UWorld* World = GetWorld())
		{
			LastParryFailTime = World->GetTimeSeconds();
		}
	}

	ParryWindowStart = 0.0;
	ParryWindowEnd = 0.0;
	bParryConsumed = false;
}

bool UChuandiAbilitySystemComponent::IsParryWindowOpen(double Now) const
{
	return UChuandiCombatMath::IsParrySuccess(ParryWindowStart, ParryWindowEnd, Now, bParryConsumed);
}
