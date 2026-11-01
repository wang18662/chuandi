#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ChuandiPlayerController.generated.h"

class UInputMappingContext;

/**
 * IMC 在这里加，不在 Character 里加（04 §11 R5：放错地方打包后输入会失效）。
 */
UCLASS()
class CHUANDI_API AChuandiPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	AChuandiPlayerController();

	virtual void SetupInputComponent() override;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> DefaultMappingContext;
};
