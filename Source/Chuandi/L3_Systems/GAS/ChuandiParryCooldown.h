#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ChuandiParryCooldown.generated.h"

/**
 * 弹反冷却 GE（0.8s）。
 * 由 UGA_PlayerParry 的 CooldownGameplayEffectClass 引用，CommitAbility 时自动施加。
 * 冷却的存在是防"无脑连按"（04 §3.3 的核心风险）。
 */
UCLASS()
class CHUANDI_API UChuandiParryCooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UChuandiParryCooldown();
};
