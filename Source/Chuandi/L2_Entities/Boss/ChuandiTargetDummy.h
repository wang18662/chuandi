#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "ChuandiTargetDummy.generated.h"

class UStaticMeshComponent;
class UChuandiAbilitySystemComponent;
class UChuandiAttributeSet;

/**
 * 方块靶子：D2 用来验证「能打能死」。
 * 有自己的 ASC + AttributeSet，血量读 TuningData 的 BossMaxHealth（500）。
 * 进关卡后从 Place Actors 拖一个 ChuandiTargetDummy 到地面上即可。
 */
UCLASS()
class CHUANDI_API AChuandiTargetDummy : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AChuandiTargetDummy();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UChuandiAttributeSet* GetChuandiAttributeSet() const { return AttributeSet; }

protected:
	virtual void BeginPlay() override;

	void HandleDeath(AActor* DeadActor);

	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TObjectPtr<UChuandiAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TObjectPtr<UChuandiAttributeSet> AttributeSet;
};
