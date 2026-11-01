#include "L3_Systems/GAS/ChuandiDamageEffect.h"

#include "L3_Systems/GAS/ChuandiDamageExecCalc.h"

UChuandiDamageEffect::UChuandiDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// 不再用 Modifier：倍率统一在 ExecCalc 里算（破防 ×2、易伤 ×1.5）
	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UChuandiDamageExecCalc::StaticClass();
	Executions.Add(Execution);
}
