#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ChuandiGameSettings.generated.h"

/**
 * Feature Flag（硬约束 C3）：任何一天打包失败，关掉没做完的模块就能出包。
 * 落盘在 Config/DefaultGame.ini 的 [/Script/Chuandi.ChuandiGameSettings]
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Chuandi"))
class CHUANDI_API UChuandiGameSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(config, EditAnywhere, Category = "Feature Flags")
	bool bEnableMeteor = true;

	UPROPERTY(config, EditAnywhere, Category = "Feature Flags")
	bool bEnableBurrow = true;

	UPROPERTY(config, EditAnywhere, Category = "Feature Flags")
	bool bEnableTrap = true;

	UPROPERTY(config, EditAnywhere, Category = "Debug")
	bool bDrawDebugShapes = true;

	static const UChuandiGameSettings& Get();
};
