#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ChuandiFrozenEffect.generated.h"

/**
 * 冰冻 GE（D22：持续 3 秒）。
 * 陷阱只控不破（D27）——它不提供任何伤害倍率，只让 Boss 停手。
 * State.Frozen 由 UChuandiCombatLibrary::ApplyStatusGEWithTags 挂到 spec 上。
 */
UCLASS()
class CHUANDI_API UChuandiFrozenEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UChuandiFrozenEffect();
};
