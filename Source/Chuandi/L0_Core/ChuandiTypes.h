#pragma once

#include "CoreMinimal.h"
#include "ChuandiTypes.generated.h"

class AActor;

/** 状态种类。GAS 下它只用于 UI 与日志，真正的状态由 GameplayTag 承载。 */
UENUM(BlueprintType)
enum class EStatusType : uint8
{
	None       UMETA(DisplayName = "无"),
	Vulnerable UMETA(DisplayName = "易伤"),
	Frozen     UMETA(DisplayName = "冰冻"),
	Slowed     UMETA(DisplayName = "减速")
};

/** Boss 状态机（由 GameplayTag 事件驱动，见 04 §6 A12） */
UENUM(BlueprintType)
enum class EBossState : uint8
{
	Idle     UMETA(DisplayName = "待机"),
	Chase    UMETA(DisplayName = "追击"),
	Casting  UMETA(DisplayName = "施法"),
	Frozen   UMETA(DisplayName = "冰冻"),
	Broken   UMETA(DisplayName = "破防"),
	Burrowed UMETA(DisplayName = "遁地"),
	Dead     UMETA(DisplayName = "死亡")
};

/** 布阱三态：Idle → Aiming →（松开释放 / X 取消）→ Idle */
UENUM(BlueprintType)
enum class ETrapPlacementState : uint8
{
	Idle   UMETA(DisplayName = "空闲"),
	Aiming UMETA(DisplayName = "瞄准中")
};

/** 一次伤害请求。Amount 是未乘倍率的基础值，倍率在 ExecCalc 里算。 */
USTRUCT(BlueprintType)
struct FDamageInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Chuandi")
	float Amount = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Chuandi")
	bool bCanParried = false;

	UPROPERTY(BlueprintReadWrite, Category = "Chuandi")
	TObjectPtr<AActor> Source = nullptr;
};

/** 陷阱配置（D22 / D28 / D33） */
USTRUCT(BlueprintType)
struct FTrapData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chuandi")
	float TriggerRadius = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chuandi")
	float EffectDuration = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chuandi")
	float Lifetime = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chuandi")
	int32 Cost = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chuandi")
	bool bConsumeOnTrigger = true;
};

/** 弹反窗口的纯数据表示。判定逻辑见 UChuandiCombatMath::IsParrySuccess。 */
USTRUCT(BlueprintType)
struct FParryWindow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Chuandi")
	double Start = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Chuandi")
	double End = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Chuandi")
	bool bConsumed = false;
};
