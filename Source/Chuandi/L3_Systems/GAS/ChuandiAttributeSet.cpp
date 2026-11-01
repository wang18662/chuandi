#include "L3_Systems/GAS/ChuandiAttributeSet.h"

#include "L0_Core/ChuandiLog.h"
#include "GameplayEffectExtension.h"

UChuandiAttributeSet::UChuandiAttributeSet()
{
	// 初值只是兜底，真实值由 TuningData 在 BeginPlay 时写入
	InitHealth(150.f);
	InitMaxHealth(150.f);
	InitMoveSpeed(500.f);
}

void UChuandiAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

void UChuandiAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute != GetHealthAttribute())
	{
		return;
	}

	SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));

	AActor* Avatar = Data.Target.GetAvatarActor();
	UE_LOG(LogChuandi, Log, TEXT("[HP] %s = %.1f / %.1f  (本次 %+.1f)"),
		*GetNameSafe(Avatar), GetHealth(), GetMaxHealth(), Data.EvaluatedData.Magnitude);

	if (GetHealth() <= 0.f && !bDeathBroadcast)
	{
		bDeathBroadcast = true;
		UE_LOG(LogChuandi, Log, TEXT("[HP] %s 死亡"), *GetNameSafe(Avatar));
		OnDeath.Broadcast(Avatar);
	}
}
