#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayEffect.h"
#include "ChuandiCombatLibrary.generated.h"

/**
 * 伤害与状态的唯一入口（ADR A7）。
 * 所有伤害必须经过这里，否则破防倍率、日志、死亡广播都会漏。
 */
UCLASS()
class CHUANDI_API UChuandiCombatLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 对目标造成伤害。返回实际施加的基础伤害（未乘倍率），失败返回 0。
	 * 内部走 GE，所以目标的 PostGameplayEffectExecute 会被触发（死亡检测在那里）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Chuandi|Combat")
	static float ApplyDamage(AActor* Source, AActor* Target, float Damage);

	/** 给人挂一个状态 GE（冰冻 / 破防 / 减速）。返回是否成功。 */
	UFUNCTION(BlueprintCallable, Category = "Chuandi|Combat")
	static bool ApplyStatusGE(AActor* Source, AActor* Target, TSubclassOf<UGameplayEffect> EffectClass, float Level = 1.f);

	/** 目标身上是否有某个 tag（查状态用，别自己拼字符串） */
	UFUNCTION(BlueprintPure, Category = "Chuandi|Combat")
	static bool HasTag(const AActor* Target, FGameplayTag Tag);

	/**
	 * 破防的唯一入口（ADR A7）。
	 * 只做一件事：给目标挂 GE_Break（授予 State.Broken，持续 2s）。
	 * 硬直由 FSM 读同一个 tag 得到，数值端由 ExecCalc 读同一个 tag 得到。
	 */
	UFUNCTION(BlueprintCallable, Category = "Chuandi|Combat")
	static bool ApplyBreak(AActor* Source, AActor* Target);

	/**
	 * 全局慢动作：现实时间 RealDuration 秒内把 dilation 压到 Dilation，然后恢复原值。
	 * 破防的"顿感"就是靠它（07 §1.5）。
	 */
	static void GlobalSlowMo(UWorld* World, float RealDuration, float Dilation);

	/**
	 * 施加一个状态 GE，并把 GrantedTag 挂到**这个 spec** 上（不是写在 GE 类里）。
	 *
	 * ★ 为什么要挂在 spec 上：
	 *   GE 的 InheritableOwnedTagsContainer 是在 CDO 构造时写入的，那时候原生 tag
	 *   （ChuandiTags::Xxx）静态对象还没注册完，写进去的是空 tag → 运行时永远不生效。
	 *   挂到 FGameplayEffectSpec::DynamicGrantedTags 上则发生在运行时，100% 可靠。
	 */
	static bool ApplyStatusGEWithTags(AActor* Source, AActor* Target, TSubclassOf<UGameplayEffect> EffectClass,
		FGameplayTag GrantedTag, float Level = 1.f, float DurationOverride = -1.f);

	/**
	 * 顿帧：真实时间 RealSeconds 内把全局时间膨胀压到 Dilation，然后自动恢复。
	 * 恢复必须用 FTSTicker（真实时间），用 FTimerManager 会被时间膨胀一起拖慢（R3）。
	 */
	static void HitStop(UWorld* World, float RealSeconds, float Dilation);
};
