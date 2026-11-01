#include "L3_Systems/GAS/Abilities/GA_PlayerDodge.h"

#include "L0_Core/ChuandiLog.h"
#include "L0_Core/ChuandiNativeTags.h"
#include "L1_Framework/ChuandiTuningData.h"
#include "L2_Entities/Player/ChuandiPlayerCharacter.h"
#include "L3_Systems/GAS/ChuandiCombatLibrary.h"
#include "L3_Systems/GAS/ChuandiParryCooldown.h"

#include "GameFramework/CharacterMovementComponent.h"

UGA_PlayerDodge::UGA_PlayerDodge()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
}

void UGA_PlayerDodge::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	AChuandiPlayerCharacter* Player = Cast<AChuandiPlayerCharacter>(GetAvatarActorFromActorInfo());
	if (!Player)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	// 冷却（手动 tag，理由同弹反：GE 类的 CDO tag 在原生 tag 注册前就写死了）
	if (UChuandiCombatLibrary::HasTag(Player, ChuandiTags::Cooldown_Dodge.GetTag()))
	{
		UE_LOG(LogChuandi, Verbose, TEXT("[Dodge] 冷却中"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 闪避可以取消后摇/硬直（07 §0：可取消后摇），所以这里不检查 IsActionLocked

	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float Distance = Tuning ? Tuning->DodgeDistance : 400.f;
	const float IFrames = Tuning ? Tuning->DodgeInvincibleTime : 0.3f;
	const float Cooldown = Tuning ? Tuning->DodgeCooldown : 0.8f;

	// 方向：输入向量 → 没有输入就用朝向
	FVector Direction = FVector::ZeroVector;
	if (const UCharacterMovementComponent* Movement = Player->GetCharacterMovement())
	{
		Direction = Movement->GetLastInputVector();
	}
	if (Direction.IsNearlyZero())
	{
		Direction = Player->GetActorForwardVector();
	}

	// 位移速度 = 距离 / 无敌帧时长（0.3s 走完 4m）
	const float Speed = Distance / FMath::Max(IFrames, 0.05f);

	Player->StartDash(Direction, IFrames, Speed, IFrames);

	// 冷却：复用通用的"有时长 GE"，tag 用 Cooldown.Dodge（时长由 spec 覆盖）
	UChuandiCombatLibrary::ApplyStatusGEWithTags(Player, Player,
		UChuandiParryCooldown::StaticClass(), ChuandiTags::Cooldown_Dodge.GetTag(), 1.f, Cooldown);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
