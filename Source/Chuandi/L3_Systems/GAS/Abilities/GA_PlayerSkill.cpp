#include "L3_Systems/GAS/Abilities/GA_PlayerSkill.h"

#include "L0_Core/ChuandiLog.h"
#include "L0_Core/ChuandiNativeTags.h"
#include "L1_Framework/ChuandiTuningData.h"
#include "L2_Entities/Boss/ChuandiBossCharacter.h"
#include "L2_Entities/Player/ChuandiPlayerCharacter.h"
#include "L3_Systems/GAS/ChuandiCombatLibrary.h"
#include "L3_Systems/GAS/ChuandiParryCooldown.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

UGA_PlayerSkill::UGA_PlayerSkill()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
}

void UGA_PlayerSkill::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	// 硬直/后摇期间放不出
	if (const AChuandiPlayerCharacter* PlayerChar = Cast<AChuandiPlayerCharacter>(Avatar))
	{
		if (PlayerChar->IsActionLocked())
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
	}

	// 8s 冷却：空放也进冷却（07 §4.3 失败态）
	if (UChuandiCombatLibrary::HasTag(Avatar, ChuandiTags::Cooldown_Skill.GetTag()))
	{
		UE_LOG(LogChuandi, Verbose, TEXT("[Skill] 冷却中"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float Damage = Tuning ? Tuning->PlayerSkillDamage : 15.f;
	const float Range = Tuning ? Tuning->PlayerSkillRange : 300.f;
	const float HalfAngle = Tuning ? Tuning->PlayerSkillHalfAngle : 45.f;
	const float Cooldown = Tuning ? Tuning->PlayerSkillCooldown : 8.f;

	UChuandiCombatLibrary::ApplyStatusGEWithTags(Avatar, Avatar,
		UChuandiParryCooldown::StaticClass(), ChuandiTags::Cooldown_Skill.GetTag(), 1.f, Cooldown);

	// 方向：角色朝向（02 §4.3「前方锥形判定」按字面实现）
	const FVector Origin = Avatar->GetActorLocation();
	const FVector Forward = Avatar->GetActorForwardVector();
	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(HalfAngle));

	TArray<AActor*> Candidates;
	UGameplayStatics::GetAllActorsWithInterface(Avatar, UAbilitySystemInterface::StaticClass(), Candidates);

	int32 HitCount = 0;
	float TotalDamage = 0.f;

	for (AActor* Candidate : Candidates)
	{
		if (!Candidate || Candidate == Avatar)
		{
			continue;
		}
		if (UChuandiCombatLibrary::HasTag(Candidate, ChuandiTags::State_Dead.GetTag()))
		{
			continue;
		}

		const FVector ToTarget = Candidate->GetActorLocation() - Origin;
		const float Dist = ToTarget.Size();
		if (Dist > Range || Dist < KINDA_SMALL_NUMBER)
		{
			continue;
		}
		if (FVector::DotProduct(Forward, ToTarget / Dist) < CosHalfAngle)
		{
			continue;
		}

		// 遮挡判定与普攻一致：隔石头打不到
		FHitResult BlockHit;
		FCollisionQueryParams BlockParams(SCENE_QUERY_STAT(SkillLOS), false, Avatar);
		const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
			BlockHit, Origin + FVector(0.f, 0.f, 50.f), Candidate->GetActorLocation(),
			ECC_Visibility, BlockParams)
			&& BlockHit.GetActor() != Candidate;
		if (bBlocked)
		{
			continue;
		}

		UChuandiCombatLibrary::ApplyDamage(Avatar, Candidate, Damage);

		// 08 §3：技能命中 → Boss 闪白（"目标有反应"才算成功）
		if (AChuandiBossCharacter* BossChar = Cast<AChuandiBossCharacter>(Candidate))
		{
			BossChar->FlashColor(FLinearColor::White, 0.08f);
		}

		TotalDamage += Damage;
		++HitCount;
	}

	if (HitCount > 0)
	{
		UE_LOG(LogChuandi, Log, TEXT("[Skill] 锥形命中 %d 个目标，每个 %.1f"), HitCount, Damage);
	}
	else
	{
		UE_LOG(LogChuandi, Log, TEXT("[Skill] 空放（前方 %.0fcm 锥形内无目标），仍进冷却"), Range);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
