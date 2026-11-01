#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_BossWave.generated.h"

class ATelegraphActor;

/**
 * 声波（D11 / D44）：
 *   1.2 秒前摇，地面上出现扩张的红色圆盘作为预警
 *   2. 到点结算：玩家在半径内 → 扣 8 血 + 减速 2 秒
 *   3. D4 会在这里插入弹反判定，现在先只做"必中"
 */
UCLASS()
class CHUANDI_API UGA_BossWave : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_BossWave();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** 技能被取消（冰冻打断）时，预警圈必须一起消失，否则玩家会误判还有一击 */
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	/** WaitDelay 到点后的命中帧（第一波） */
	UFUNCTION()
	void OnCastFinished();

	/** 第二 / 三波 */
	UFUNCTION()
	void OnNextWave();

	/** 释放一波并结算（07 §3.3：三波必须是独立的三次伤害判定） */
	void ReleaseWave();

	/** 波前到达玩家时刻的实际结算 */
	void ApplyWaveHit(AActor* Boss, APawn* Player, float Radius, float Damage, int32 WaveIndex);

	/** 三波放完：回状态机 + 结束 */
	void FinishCast();

	UPROPERTY(Transient)
	TObjectPtr<ATelegraphActor> Telegraph;

	float CachedRadius = 700.f;
	float CachedDamage = 8.f;
	float CachedInterval = 0.6f;
	int32 CachedWaveCount = 3;
	int32 WavesReleased = 0;
};
