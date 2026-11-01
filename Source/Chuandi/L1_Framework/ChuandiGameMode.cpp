#include "L1_Framework/ChuandiGameMode.h"

#include "L0_Core/ChuandiLog.h"
#include "L1_Framework/ChuandiHUD.h"
#include "L1_Framework/ChuandiPlayerController.h"
#include "L2_Entities/Player/ChuandiPlayerCharacter.h"

#include "Engine/Engine.h"

AChuandiGameMode::AChuandiGameMode()
{
	DefaultPawnClass = AChuandiPlayerCharacter::StaticClass();
	PlayerControllerClass = AChuandiPlayerController::StaticClass();
	// 注意：UE5.8 起 CheatClass 在 APlayerController 上，不在 GameMode（见 PlayerController 构造函数）
	HUDClass = AChuandiHUD::StaticClass();
}

void AChuandiGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (const UWorld* World = GetWorld())
	{
		StartTimeSeconds = World->GetTimeSeconds();
	}
}

float AChuandiGameMode::GetElapsedSeconds() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.f;
	}
	const double End = (EndTimeSeconds >= 0.0) ? EndTimeSeconds : World->GetTimeSeconds();
	return static_cast<float>(FMath::Max(0.0, End - StartTimeSeconds));
}

float AChuandiGameMode::GetParryRate() const
{
	if (ParryAttempts <= 0)
	{
		return -1.f;
	}
	return static_cast<float>(ParrySuccesses) / static_cast<float>(ParryAttempts);
}

FString AChuandiGameMode::GetRank() const
{
	const float Rate = GetParryRate();
	if (Rate < 0.f)
	{
		return TEXT("C");
	}
	if (Rate >= 0.8f && !bDefeat)
	{
		return TEXT("S");
	}
	if (Rate >= 0.6f)
	{
		return TEXT("A");
	}
	if (Rate >= 0.4f)
	{
		return TEXT("B");
	}
	return TEXT("C");
}

void AChuandiGameMode::RegisterTarget(AActor* InTarget)
{
	if (!InTarget)
	{
		return;
	}

	PrimaryTarget = InTarget;
	UE_LOG(LogChuandi, Log, TEXT("[GM] 主目标注册: %s"), *GetNameSafe(InTarget));
}

void AChuandiGameMode::NotifyTargetDied(AActor* DeadTarget)
{
	if (bVictory)
	{
		return;
	}

	bVictory = true;
	if (const UWorld* World = GetWorld())
	{
		EndTimeSeconds = World->GetTimeSeconds();
	}

	UE_LOG(LogChuandi, Log,
		TEXT("[GM] 胜利：用时 %.1fs / 弹反 %d/%d（%.0f%%）/ 破防 %d 次 / 承伤 %.0f / 评级 %s"),
		GetElapsedSeconds(), ParrySuccesses, ParryAttempts,
		GetParryRate() < 0.f ? 0.f : GetParryRate() * 100.f,
		BreakCount, DamageTaken, *GetRank());
}

void AChuandiGameMode::NotifyPlayerDied()
{
	if (bDefeat)
	{
		return;
	}

	bDefeat = true;
	if (const UWorld* World = GetWorld())
	{
		EndTimeSeconds = World->GetTimeSeconds();
	}

	UE_LOG(LogChuandi, Log,
		TEXT("[GM] 失败：用时 %.1fs / 弹反 %d/%d / 破防 %d 次 / 承伤 %.0f / 评级 %s"),
		GetElapsedSeconds(), ParrySuccesses, ParryAttempts, BreakCount, DamageTaken, *GetRank());
}
