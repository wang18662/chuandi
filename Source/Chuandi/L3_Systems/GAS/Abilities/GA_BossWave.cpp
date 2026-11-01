#include "L3_Systems/GAS/Abilities/GA_BossWave.h"

#include "L0_Core/ChuandiLog.h"
#include "L1_Framework/ChuandiTuningData.h"
#include "L2_Entities/Boss/ChuandiBossCharacter.h"
#include "L2_Entities/World/CoverActor.h"
#include "L2_Entities/World/TelegraphActor.h"
#include "L2_Entities/World/WaveRingActor.h"
#include "L3_Systems/BossStateMachineComponent.h"
#include "L3_Systems/BreakMeterComponent.h"
#include "L3_Systems/GAS/ChuandiCombatLibrary.h"
#include "L3_Systems/GAS/ChuandiSlowEffect.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "L3_Systems/GAS/ChuandiAbilitySystemComponent.h"

UGA_BossWave::UGA_BossWave()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
}

void UGA_BossWave::ActivateAbility(
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

	AActor* Boss = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!Boss || !World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float CastTime = Tuning ? Tuning->WaveCastTime : 1.2f;
	CachedRadius = Tuning ? Tuning->WaveRadius : 700.f;
	CachedDamage = Tuning ? Tuning->WaveDamage : 8.f;

	// ★ 每次施法都要清零：能力是 InstancedPerActor，成员会跨次复用。
	//   不清零的话第二次施法从 3 起算 → "第 4/3 波"，而且只放一波就结束。
	WavesReleased = 0;

	// 预警圈：半径随前摇扩张，到点自己销毁
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Boss;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Telegraph = World->SpawnActor<ATelegraphActor>(
		ATelegraphActor::StaticClass(), Boss->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);

	if (Telegraph)
	{
		// 04 §2.1：声波 = 紫 #8B5CF6
		Telegraph->PlayWarning(CastTime, CachedRadius, FLinearColor(0.55f, 0.36f, 0.96f, 1.f));
	}

	UE_LOG(LogChuandi, Log, TEXT("[Wave] 施法开始，前摇 %.2fs，半径 %.0f，三波配置=次数 %d / 间隔 %.2f / 波速 %.0f"),
		CastTime, CachedRadius,
		Tuning ? Tuning->WaveCount : 3, Tuning ? Tuning->WaveInterval : 0.6f,
		Tuning ? Tuning->WaveSpeed : 1500.f);

	// 前摇 → 命中帧（时间基准与弹反判定同源，见 04 §5.1）
	UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, CastTime);
	DelayTask->OnFinish.AddDynamic(this, &UGA_BossWave::OnCastFinished);
	DelayTask->ReadyForActivation();
}

void UGA_BossWave::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// 预警圈的寿命跟技能绑定：技能结束（无论正常还是被打断）都把圈收掉
	if (IsValid(Telegraph))
	{
		Telegraph->Destroy();
	}
	Telegraph = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_BossWave::OnCastFinished()
{
	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	CachedWaveCount = Tuning ? Tuning->WaveCount : 3;
	CachedInterval = Tuning ? Tuning->WaveInterval : 0.6f;

	ReleaseWave();

	if (WavesReleased < CachedWaveCount)
	{
		UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, CachedInterval);
		DelayTask->OnFinish.AddDynamic(this, &UGA_BossWave::OnNextWave);
		DelayTask->ReadyForActivation();
	}
	else
	{
		FinishCast();
	}
}

void UGA_BossWave::OnNextWave()
{
	ReleaseWave();

	if (WavesReleased < CachedWaveCount)
	{
		UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, CachedInterval);
		DelayTask->OnFinish.AddDynamic(this, &UGA_BossWave::OnNextWave);
		DelayTask->ReadyForActivation();
	}
	else
	{
		FinishCast();
	}
}

void UGA_BossWave::ReleaseWave()
{
	++WavesReleased;

	UWorld* World = GetWorld();
	AActor* Boss = GetAvatarActorFromActorInfo();
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!World || !Boss || !Player)
	{
		return;
	}

	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float Speed = Tuning ? Tuning->WaveSpeed : 1500.f;

	// 波体：一圈向外扩的环（04 §1 声波用圆环表达"波"；§2.1 颜色 = 紫 #8B5CF6）
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Boss;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AWaveRingActor* Ring = World->SpawnActor<AWaveRingActor>(
		AWaveRingActor::StaticClass(), Boss->GetActorLocation() + FVector(0.f, 0.f, 20.f),
		FRotator::ZeroRotator, SpawnParams))
	{
		Ring->Init(CachedRadius, Speed, FLinearColor(0.55f, 0.36f, 0.96f, 1.f));
	}

	// 命中时刻 = 波前到达玩家的时刻（07 §3.3 时间线：波速 15m/s，6m 需 0.4s）
	const float Dist = FVector::Dist(Boss->GetActorLocation(), Player->GetActorLocation());
	const float Delay = FMath::Clamp(Dist / Speed, 0.f, 0.5f);
	const int32 WaveIndex = WavesReleased;

	TWeakObjectPtr<UGA_BossWave> WeakSelf(this);
	TWeakObjectPtr<AActor> WeakBoss(Boss);
	TWeakObjectPtr<APawn> WeakPlayer(Player);
	const float Radius = CachedRadius;
	const float Damage = CachedDamage;

	FTimerDelegate Delegate = FTimerDelegate::CreateLambda(
		[WeakSelf, WeakBoss, WeakPlayer, Radius, Damage, WaveIndex]()
		{
			if (!WeakSelf.IsValid())
			{
				return;
			}
			WeakSelf->ApplyWaveHit(WeakBoss.Get(), WeakPlayer.Get(), Radius, Damage, WaveIndex);
		});

	if (Delay < KINDA_SMALL_NUMBER)
	{
		Delegate.Execute();
	}
	else
	{
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, Delegate, Delay, false);
	}
}

void UGA_BossWave::ApplyWaveHit(AActor* Boss, APawn* Player, float Radius, float Damage, int32 WaveIndex)
{
	if (!Boss || !Player)
	{
		return;
	}

	const float Dist = FVector::Dist(Boss->GetActorLocation(), Player->GetActorLocation());
	if (Dist > Radius)
	{
		UE_LOG(LogChuandi, Log, TEXT("[Wave] 第 %d 波未命中：玩家在半径外（%.0f > %.0f）"),
			WaveIndex, Dist, Radius);
		return;
	}

	// 07 §3.3：三波间隔 0.6s = 跳跃滞空 0.6s —— 滞空期间躲过这一波
	const ACharacter* PlayerChar = Cast<ACharacter>(Player);
	if (PlayerChar && PlayerChar->GetCharacterMovement()
		&& PlayerChar->GetCharacterMovement()->IsFalling())
	{
		UE_LOG(LogChuandi, Log, TEXT("[Wave] 第 %d/%d 波被跳跃躲过（滞空）"), WaveIndex, CachedWaveCount);
		return;
	}

	// D7：路径上有碎石掩体 → 掩体碎、玩家不受伤（掩体一次性）
	FHitResult CoverHit;
	FCollisionQueryParams CoverParams(SCENE_QUERY_STAT(SoundWavePath), false, Boss);
	CoverParams.AddIgnoredActor(Boss);
	if (GetWorld()->LineTraceSingleByChannel(
		CoverHit,
		Boss->GetActorLocation() + FVector(0.f, 0.f, 100.f),
		Player->GetActorLocation(),
		ECC_Visibility,
		CoverParams))
	{
		if (AChuandiCoverActor* Cover = Cast<AChuandiCoverActor>(CoverHit.GetActor()))
		{
			Cover->BreakCover();
			UE_LOG(LogChuandi, Log, TEXT("[Wave] 第 %d 波被碎石掩体挡下"), WaveIndex);
			return;
		}
	}

	// 07 §3.3（11:25 版）：声波**不可弹反**，只有陨石能弹
	UChuandiCombatLibrary::ApplyDamage(Boss, Player, Damage);
	UChuandiCombatLibrary::ApplyStatusGE(Boss, Player, UChuandiSlowEffect::StaticClass());
	UE_LOG(LogChuandi, Log, TEXT("[Wave] 第 %d/%d 波命中，距离 %.0fcm，伤害 %.1f（不可弹反）"),
		WaveIndex, CachedWaveCount, Dist, Damage);
}

void UGA_BossWave::FinishCast()
{
	AActor* Boss = GetAvatarActorFromActorInfo();

	UE_LOG(LogChuandi, Log, TEXT("[Wave] 三波结束，回状态机"));

	// 通知状态机回待机，否则它会一直以为自己在施法
	if (AChuandiBossCharacter* BossChar = Cast<AChuandiBossCharacter>(Boss))
	{
		if (UBossStateMachineComponent* FSM = BossChar->GetStateMachine())
		{
			FSM->NotifyCastFinished();
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
