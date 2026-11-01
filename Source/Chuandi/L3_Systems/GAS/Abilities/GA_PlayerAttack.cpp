#include "L3_Systems/GAS/Abilities/GA_PlayerAttack.h"

#include "L0_Core/ChuandiLog.h"
#include "L0_Core/ChuandiNativeTags.h"
#include "L1_Framework/ChuandiTuningData.h"
#include "L2_Entities/Boss/ChuandiBossCharacter.h"
#include "L2_Entities/Player/ChuandiPlayerCharacter.h"
#include "L3_Systems/BreakMeterComponent.h"
#include "L3_Systems/GAS/ChuandiCombatLibrary.h"
#include "L3_Systems/GAS/ChuandiParryCooldown.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

UGA_PlayerAttack::UGA_PlayerAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
}

void UGA_PlayerAttack::ActivateAbility(
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

	// 07 §0：空中不能攻击；硬直/后摇期间也不能
	if (const AChuandiPlayerCharacter* PlayerChar = Cast<AChuandiPlayerCharacter>(Avatar))
	{
		if (PlayerChar->IsActionLocked() || PlayerChar->GetCharacterMovement()->IsFalling())
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
	}

	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float Damage = Tuning ? Tuning->PlayerAttackDamage : 3.f;
	const float Range = Tuning ? Tuning->PlayerAttackRange : 200.f;
	const float Cooldown = Tuning ? Tuning->PlayerAttackCooldown : 0.6f;

	// 普攻 0.6s 冷却（07：破防 3 秒里正好 5 刀）
	if (UChuandiCombatLibrary::HasTag(Avatar, ChuandiTags::Cooldown_Attack.GetTag()))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 空挥也进冷却——否则玩家会靠"连点"来赌命中
	UChuandiCombatLibrary::ApplyStatusGEWithTags(Avatar, Avatar,
		UChuandiParryCooldown::StaticClass(), ChuandiTags::Cooldown_Attack.GetTag(), 1.f, Cooldown);

	// 软锁定：挥刀瞬间自动转向最近的敌人。
	// 为什么需要：闪避会把角色转向移动方向（bOrientRotationToMovement），
	// 背对 Boss 挥刀时 100 度锥形判定必然落空 —— 玩家会觉得"刀挥了但没伤害"。
	// 注意：只转向，判定仍是"角色前方锥形"（07 §4.3），不是相机朝向。
	TArray<AActor*> Candidates;
	UGameplayStatics::GetAllActorsWithInterface(Avatar, UAbilitySystemInterface::StaticClass(), Candidates);

	{
		const FVector OriginNow = Avatar->GetActorLocation();
		AActor* Nearest = nullptr;
		float NearestDist = TNumericLimits<float>::Max();
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
			const float D = FVector::Dist(OriginNow, Candidate->GetActorLocation());
			if (D <= Range * 1.2f && D < NearestDist)
			{
				NearestDist = D;
				Nearest = Candidate;
			}
		}

		if (Nearest)
		{
			const FVector Dir = (Nearest->GetActorLocation() - OriginNow).GetSafeNormal2D();
			if (!Dir.IsNearlyZero())
			{
				Avatar->SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));
			}
		}
	}

	const FVector Origin = Avatar->GetActorLocation();
	const FVector Forward = Avatar->GetActorForwardVector();
	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(50.f));

	// 找范围内、正前方、最近的带 ASC 目标（现在的靶子、以后的 Boss 都适用）
	AActor* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();

	for (AActor* Candidate : Candidates)
	{
		if (!Candidate || Candidate == Avatar)
		{
			continue;
		}

		// 尸体不打
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
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = Candidate;
		}
	}

	if (Best)
	{
		// 遮挡检查：隔着石头/墙角打不到（否则"躲掩体后面无伤打 Boss"就成了最优解）
		FHitResult BlockHit;
		FCollisionQueryParams BlockParams(SCENE_QUERY_STAT(MeleeLOS), false, Avatar);
		BlockParams.AddIgnoredActor(Avatar);

		const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
			BlockHit, Origin + FVector(0.f, 0.f, 50.f), Best->GetActorLocation(), ECC_Visibility, BlockParams)
			&& BlockHit.GetActor() != Best;

		if (bBlocked)
		{
			UE_LOG(LogChuandi, Log, TEXT("[Attack] 被 %s 遮挡，未命中"),
				*GetNameSafe(BlockHit.GetActor()));
		}
		else
		{
			UChuandiCombatLibrary::ApplyDamage(Avatar, Best, Damage);
			UE_LOG(LogChuandi, Log, TEXT("[Attack] 命中 %s，伤害 %.1f（距离 %.0fcm）"),
				*GetNameSafe(Best), Damage, BestDist);

			// 07 §1.8：普攻命中 +1 破防条（保底来源）
			// 08 §3：命中要有"目标反应" —— 受击闪白（04 §4：0.08s）
			if (AChuandiBossCharacter* BossChar = Cast<AChuandiBossCharacter>(Best))
			{
				BossChar->FlashColor(FLinearColor::White, 0.08f);

				if (UBreakMeterComponent* Meter = BossChar->GetBreakMeter())
				{
					Meter->AddGauge(Tuning ? Tuning->BreakGaugeOnAttack : 1.f);
				}
			}
		}
	}
	else
	{
		UE_LOG(LogChuandi, Verbose, TEXT("[Attack] 落空（范围内没有目标）"));
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
