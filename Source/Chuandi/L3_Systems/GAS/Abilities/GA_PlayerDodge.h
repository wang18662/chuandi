#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_PlayerDodge.generated.h"

/**
 * 闪避（07 §0）：4m 位移 / 0.3s 无敌帧 / 0.8s 冷却。
 * 位移交给角色 Tick 做（见 AChuandiPlayerCharacter::StartDash），
 * 因为 Ability 不适合逐帧推位移，而角色 Tick 天然有 delta 与碰撞。
 */
UCLASS()
class CHUANDI_API UGA_PlayerDodge : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_PlayerDodge();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
