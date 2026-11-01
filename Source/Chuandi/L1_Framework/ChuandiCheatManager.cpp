#include "L1_Framework/ChuandiCheatManager.h"

#include "L0_Core/ChuandiLog.h"
#include "L3_Systems/GAS/ChuandiAbilitySystemComponent.h"
#include "L3_Systems/GAS/ChuandiAttributeSet.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"

UAbilitySystemComponent* UChuandiCheatManager::GetPlayerASC() const
{
	const APlayerController* PC = GetOuterAPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return nullptr;
	}

	return Pawn->FindComponentByClass<UChuandiAbilitySystemComponent>();
}

void UChuandiCheatManager::DumpTags()
{
	UAbilitySystemComponent* ASC = GetPlayerASC();
	if (!ASC)
	{
		UE_LOG(LogChuandi, Warning, TEXT("[GAS] 玩家身上没有 ASC"));
		return;
	}

	FGameplayTagContainer Tags;
	ASC->GetOwnedGameplayTags(Tags);

	UE_LOG(LogChuandi, Log, TEXT("──────── DumpTags ────────"));
	UE_LOG(LogChuandi, Log, TEXT("Avatar : %s"), *GetNameSafe(ASC->GetAvatarActor()));
	UE_LOG(LogChuandi, Log, TEXT("Tags   : %s"), Tags.IsEmpty() ? TEXT("(empty)") : *Tags.ToStringSimple());

	if (const UChuandiAttributeSet* Attr = ASC->GetSet<UChuandiAttributeSet>())
	{
		UE_LOG(LogChuandi, Log, TEXT("Health : %.1f / %.1f"),
			Attr->GetHealth(), Attr->GetMaxHealth());
		UE_LOG(LogChuandi, Log, TEXT("Speed  : %.1f"), Attr->GetMoveSpeed());
	}
	else
	{
		UE_LOG(LogChuandi, Warning, TEXT("[GAS] ASC 上没有挂 UChuandiAttributeSet"));
	}
}
