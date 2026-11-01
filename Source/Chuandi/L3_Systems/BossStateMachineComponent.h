#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "L0_Core/ChuandiTypes.h"
#include "BossStateMachineComponent.generated.h"

class AChuandiBossCharacter;

/**
 * Boss 决策层（04 §6 A12：由 tag 事件驱动，不轮询）。
 *
 * D3 只需要最小闭环：站着 → 到冷却且玩家在射程内 → 施法声波 → 回到待机。
 * D6 再接感知与追击，D8 加遁地。
 */
UCLASS(ClassGroup = (Chuandi), meta = (BlueprintSpawnableComponent))
class CHUANDI_API UBossStateMachineComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossStateMachineComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	EBossState GetState() const { return CurrentState; }
	void SetState(EBossState NewState);

	/** 施法结束由 Ability 回调，通知状态机回到待机 */
	void NotifyCastFinished();

	/**
	 * 受击唤醒：忽略视野，直接把攻击者位置当成"最后已知位置"并转入追击。
	 * 这是堵住"从背后无伤站桩打 Boss"的关键一环。
	 */
	void NotifyDamaged(AActor* Attacker);

	/**
	 * 破防条满的瞬间调用：立刻切到 Broken（不能等 5Hz 的下一拍，
	 * 07 校验点 B-01 要求"条满到破防"≤1 帧）。
	 */
	void NotifyBreakTriggered();

protected:
	void Evaluate();

	UPROPERTY(VisibleInstanceOnly, Category = "Chuandi")
	EBossState CurrentState = EBossState::Idle;

	/** 声波 8s / 陨石 12s 各自独立冷却（交接 §1） */
	double NextWaveTime = 0.0;
	double NextMeteorTime = 0.0;
	/** 两个都就绪时用来交替，避免连续同一招 */
	bool bNextSkillIsMeteor = false;
	double NextMeleeTime = 0.0;

	/** 牵引 18s / 火焰 10s，各自独立冷却（10-29 新增的两个技能） */
	double NextPullTime = 0.0;
	double NextFlameTime = 0.0;

	/** 全屏秒杀 35s / 冲撞破柱 14s（10-29 新增） */
	double NextInstaKillTime = 0.0;
	double NextChargeTime = 0.0;

	// ── 技能解锁节奏（07 §3.3-C）──
	/** 0=近战/声波/陨石　1=+牵引　2=+遁地/火焰　3=+秒杀/冲撞 */
	int32 CurrentTier = 0;
	/** 追击累计时长（秒）：Tier1 的时间兜底 */
	float ChaseAccum = 0.f;

	/** 本次施法开始的时刻（0 = 没在施法）：用来做"卡死兜底" */
	double CastingSince = 0.0;
	/** 解锁后强制下一个释放的技能：0=无 1=牵引 2=遁地 3=全屏秒杀 */
	int32 ForcedSkill = 0;

	/** 按血量/追击时长推进解锁档位；返回是否刚刚升档 */
	bool UpdateSkillTier(float HealthRatio, double Now, AChuandiBossCharacter* Boss);
	bool bCasting = false;
	/** 遁地只触发一次（07 §3.2：血量 ≤60% 且未遁过） */
	bool bBurrowedOnce = false;

	/** 狂暴期（<20%）遁地的下一次可触发时刻：此时遁地进入常规冷却（02 §6.4） */
	double NextBurrowTime = 0.0;

	/** 阶段提示只触发一次（40% 陨石加密 / 20% 狂暴） */
	bool bDensePhaseAnnounced = false;
	bool bEnrageAnnounced = false;

	float DebugAccum = 0.f;
};
