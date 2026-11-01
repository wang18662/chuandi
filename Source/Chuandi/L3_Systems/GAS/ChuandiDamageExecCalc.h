#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "ChuandiDamageExecCalc.generated.h"

/**
 * 伤害倍率的唯一计算点（ADR A7）。
 *
 * 为什么倍率一定要放在 ExecCalc，而不是在 C++ 里乘一下再传：
 *   倍率规则（破防 ×2、易伤 ×1.5、以后可能的抗性）会越来越多，
 *   散在各个技能里必然漏乘。集中在这里，加一条规则只改一个 if。
 *
 * 约定：伤害由 SetByCaller(Data.Damage) 传入，**负值代表扣血**。
 */
UCLASS()
class CHUANDI_API UChuandiDamageExecCalc : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UChuandiDamageExecCalc();

	virtual void Execute_Implementation(
		const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
