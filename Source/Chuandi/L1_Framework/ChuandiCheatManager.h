#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "ChuandiCheatManager.generated.h"

/**
 * 控制台调试命令（Development 包按 ~ 呼出）。
 * 注意：CheatManager 的 Exec 函数是「直接输入函数名」，不加 Chuandi. 前缀。
 */
UCLASS()
class CHUANDI_API UChuandiCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	/** 打印玩家 ASC 的所有 tag + 血量（V1/V4 的取证工具，保留） */
	UFUNCTION(Exec)
	void DumpTags();

private:
	class UAbilitySystemComponent* GetPlayerASC() const;
};
