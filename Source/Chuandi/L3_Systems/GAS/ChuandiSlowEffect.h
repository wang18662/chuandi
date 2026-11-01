#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ChuandiSlowEffect.generated.h"

/**
 * 减速 GE（Duration 2s，MoveSpeed -250）。
 * 数值是常量，直接写在这里——按 04 §7.1 的规则："只被 GAS 用的数放 GE 里"。
 * 声波命中后给玩家挂这个，到期自动恢复。
 */
UCLASS()
class CHUANDI_API UChuandiSlowEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UChuandiSlowEffect();
};
