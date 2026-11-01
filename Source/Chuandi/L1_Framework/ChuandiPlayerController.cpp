#include "L1_Framework/ChuandiPlayerController.h"

#include "L0_Core/ChuandiLog.h"
#include "L1_Framework/ChuandiCheatManager.h"

#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"

namespace
{
	const TCHAR* DefaultIMCPath = TEXT("/Game/Chuandi/Input/IMC_Default.IMC_Default");
}

AChuandiPlayerController::AChuandiPlayerController()
{
	// UE5.8 起 CheatClass 在本类上（不在 AGameModeBase）
	CheatClass = UChuandiCheatManager::StaticClass();
}

void AChuandiPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!IsLocalPlayerController())
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!Subsystem)
	{
		UE_LOG(LogChuandi, Error, TEXT("[Input] 拿不到 EnhancedInputLocalPlayerSubsystem"));
		return;
	}

	DefaultMappingContext = Cast<UInputMappingContext>(
		StaticLoadObject(UInputMappingContext::StaticClass(), nullptr, DefaultIMCPath));

	if (!DefaultMappingContext)
	{
		UE_LOG(LogChuandi, Warning,
			TEXT("[Input] 找不到 %s。请在 Content/Chuandi/Input/ 下建 IMC_Default。"), DefaultIMCPath);
		return;
	}

	Subsystem->AddMappingContext(DefaultMappingContext, /*Priority=*/0);
	UE_LOG(LogChuandi, Log, TEXT("[Input] 已挂载 IMC: %s"), *DefaultMappingContext->GetName());
}
