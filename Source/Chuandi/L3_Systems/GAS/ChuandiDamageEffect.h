#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ChuandiDamageEffect.generated.h"

/**
 * 伤害 GE（Instant）。
 *
 * 为什么在 C++ 里构造、不做成资产：
 *   伤害只有"扣血"一种效果，配置项为零。做成 uasset 只会多一个要手点、要跟着改的资产。
 *
 * 伤害值怎么传：SetByCaller（tag = Data.Damage）。
 *   ★ 不要用 ScalableFloat + GE Level：FScalableFloat 的 Value 是"系数"，
 *     没有曲线表时它不会乘以 Level，结果永远是系数本身（踩过：伤害恒为 1）。
 */
UCLASS()
class CHUANDI_API UChuandiDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UChuandiDamageEffect();
};
