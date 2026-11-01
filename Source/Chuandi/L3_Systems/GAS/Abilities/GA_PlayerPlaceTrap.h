#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_PlayerPlaceTrap.generated.h"

/**
 * 布阱（D5）：消耗 1 个传递物，在角色前方 150cm 的地面放一个陷阱。
 * 三态（Aiming / Release / Cancel）在灰盒阶段不做——先验证"放了能冻住"。
 */
UCLASS()
class CHUANDI_API UGA_PlayerPlaceTrap : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_PlayerPlaceTrap();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
