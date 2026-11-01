#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ChuandiBreakEffect.generated.h"

/**
 * 破防 GE（D22 / D27）：持续 2 秒，期间授予 State.Broken。
 *
 * 破防 = 两件事，各由一个系统负责：
 *   数值端：State.Broken 被 ExecCalc 读到 → 伤害 ×2
 *   行为端：FSM 读到 State.Broken → 停机硬直
 * 两边都靠同一个 tag，不会出现"扣了双倍但怪还在动"的错位。
 */
UCLASS()
class CHUANDI_API UChuandiBreakEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UChuandiBreakEffect();
};
