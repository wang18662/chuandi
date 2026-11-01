#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ChuandiAttributeSet.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FChuandiOnDeath, AActor* /*DeadActor*/);

/**
 * 玩家与 Boss 共用一套属性。
 * PostGameplayEffectExecute 是「伤害落地」的唯一出口：
 * 死亡检测、V4/V8 的打点、逐次伤害日志都放这里（04 §4.3）。
 */
UCLASS()
class CHUANDI_API UChuandiAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UChuandiAttributeSet();

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS_BASIC(UChuandiAttributeSet, Health);

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS_BASIC(UChuandiAttributeSet, MaxHealth);

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS_BASIC(UChuandiAttributeSet, MoveSpeed);

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	FChuandiOnDeath OnDeath;

private:
	bool bDeathBroadcast = false;
};
