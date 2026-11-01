#include "L2_Entities/Boss/ChuandiTargetDummy.h"

#include "L0_Core/ChuandiLog.h"
#include "L1_Framework/ChuandiGameMode.h"
#include "L1_Framework/ChuandiTuningData.h"
#include "L3_Systems/GAS/ChuandiAbilitySystemComponent.h"
#include "L3_Systems/GAS/ChuandiAttributeSet.h"

#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AChuandiTargetDummy::AChuandiTargetDummy()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;

	// 引擎自带基础几何体，不需要你建任何资产
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeAsset.Succeeded())
	{
		Mesh->SetStaticMesh(CubeAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatAsset(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MatAsset.Succeeded())
	{
		Mesh->SetMaterial(0, MatAsset.Object);
	}

	Mesh->SetCollisionProfileName(TEXT("BlockAll"));

	AbilitySystemComponent = CreateDefaultSubobject<UChuandiAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(false);

	AttributeSet = CreateDefaultSubobject<UChuandiAttributeSet>(TEXT("AttributeSet"));
	AbilitySystemComponent->AddAttributeSetSubobject(AttributeSet.Get());
}

UAbilitySystemComponent* AChuandiTargetDummy::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AChuandiTargetDummy::BeginPlay()
{
	Super::BeginPlay();

	if (!AbilitySystemComponent || !AttributeSet)
	{
		UE_LOG(LogChuandi, Error, TEXT("[Target] ASC 或 AttributeSet 没创建出来"));
		return;
	}

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	const UChuandiTuningData* Tuning = UChuandiTuningData::Get();
	const float MaxHP = Tuning ? Tuning->BossMaxHealth : 500.f;
	AttributeSet->SetMaxHealth(MaxHP);
	AttributeSet->SetHealth(MaxHP);

	AttributeSet->OnDeath.AddUObject(this, &AChuandiTargetDummy::HandleDeath);

	if (AChuandiGameMode* GM = Cast<AChuandiGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->RegisterTarget(this);
	}

	UE_LOG(LogChuandi, Log, TEXT("[Target] 靶子就绪 %s  HP=%.1f"), *GetNameSafe(this), MaxHP);
}

void AChuandiTargetDummy::HandleDeath(AActor* DeadActor)
{
	UE_LOG(LogChuandi, Log, TEXT("[Target] %s 已被击破 → 胜利条件达成"), *GetNameSafe(DeadActor));

	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetVisibility(false);

	if (AChuandiGameMode* GM = Cast<AChuandiGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->NotifyTargetDied(this);
	}

	SetLifeSpan(0.5f);
}
