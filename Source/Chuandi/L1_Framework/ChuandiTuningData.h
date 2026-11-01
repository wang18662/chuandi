#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ChuandiTuningData.generated.h"

/**
 * 跨系统的数值放这里（04 §7.1）。
 * 只被 GAS 用的数（伤害、GE 时长）不放这里，放 GE 资产里。
 *
 * 数值来源：07_规格正式版 v1.0。改动时请同步文档，别让两者漂移。
 */
UCLASS(BlueprintType)
class CHUANDI_API UChuandiTuningData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ── 弹反（07 §1） ──
	// 约束：ParryCooldown > ParryWindow，否则窗口首尾相接、连按必胜（02 §3.3）
	// 原理：连按的窗口序列是 [0,W]、[C,C+W]…，盲区长度 = C − W；C ≤ W 时无盲区
	// 当前：1.2 > 1.0 → 盲区 0.2s ✓
	// 注意：窗口 1.0 / 声波前摇 1.2 = 83%，单次容错已很高，这是"放宽到 1s"的既定代价
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parry", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float ParryWindow = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parry")
	float ParryCooldown = 1.2f;

	/** 弹反成功的后摇：不可行动（07 §1.2：状态总时长 1.1s = 1.0 窗口 + 0.1 后摇） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parry")
	float ParryRecovery = 0.1f;

	/** 弹反失败硬直（07 §1.2 采用"替换"：窗口结束直接进硬直） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parry")
	float ParryFailStun = 0.3f;

	// ── 破防条（07 §1.8）──
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BreakMeter")
	float BreakMeterMax = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BreakMeter")
	float BreakGaugeOnParry = 20.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BreakMeter")
	float BreakGaugeOnAttack = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BreakMeter")
	float BreakGaugeOnTrap = 15.f;

	/** 破防持续（10-29 下调：5s → 3s —— 破防现在主要靠"撞柱子"，次数会变多，单次收益必须降） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BreakMeter")
	float BreakDuration = 3.f;

	/** 破防期伤害倍率（10-29 下调 12 → 8：普攻 5 → 40；技能 15 → 120） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BreakMeter")
	float BreakDamageMultiplier = 8.f;

	/** 破防进入瞬间的顿帧（现实秒）—— 07 §1.8 11:53 版：不做全局时间膨胀，只顿 0.1s */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BreakMeter")
	float BreakHitStopRealTime = 0.1f;

	// ── 声波（07 §3.3）──
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
	float WaveCastTime = 1.2f;

	/** 每波 8 伤（交接 §1：三波连环，每波 8 伤，不是一次 24 伤） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
	float WaveDamage = 8.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
	float WaveCooldown = 8.f;

	/**
	 * AOE 半径 —— 40×40 场地取 20m：覆盖 π×400 = 1257㎡ / 1600㎡ = 79%，
	 * 玩家跑到边缘（距中心 28.3m）才能完全躲开，实际"跑不出去"。
	 * 07 §3.3 原值 15m 是按 25×25 场地算的，已随场地放大调整。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "100.0"))
	float WaveRadius = 2000.f;

	/**
	 * 三波 × 8（07 §3.3 11:33 版）：间隔 0.6s = 跳跃滞空 0.6s，
	 * 逼玩家"跟着节拍连跳"；三波是独立三次判定，不是一次 24 伤。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss")
	int32 WaveCount = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss")
	float WaveInterval = 0.6f;

	/**
	 * 波速 15m/s（30m 半径需 2s 到边缘；07 §3.3 原按 6m 半径算的是 0.4s）
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss")
	float WaveSpeed = 1500.f;

	/**
	 * 声波的最小施放距离 6m（10-29 用户要求："被拉过来后不要放声波"）。
	 * 原因：声波是**从 Boss 向外扩的环**，贴身放时环 0.2s 就到人，
	 * 玩家没有 1.2s 前摇的可读时间，三波也来不及连跳 → 等于不可躲。
	 * 近身一律交给近战/牵引，声波只在 6m 外用。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss")
	float WaveMinRange = 600.f;

	// ── 陨石 / 掩体（07 §3.3 / §4.2）──
	// 预告时长与落点半径用下面「陨石」段落的 MeteorWarningTime / MeteorRadius
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Meteor")
	float MeteorDamage = 15.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Meteor")
	float MeteorCooldown = 12.f;

	/** 【3 落点时代的残留值】现改为全屏 MeteorCircleCount 个圈，此参数可能已不再被引用，确认后可删 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Meteor")
	float MeteorScatterRadius = 350.f;

	/** 场上碎石上限（10-29：柱子是战略资源，上限从 6 提到 10，避免刚生成的柱子被挤掉） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Meteor")
	int32 CoverMaxCount = 10;

	// ── Boss 近战（07 §3.3）──
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	float MeleeRange = 200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	float MeleeDamage = 4.f;

	/** 前摇 1.2s（11-01 用户定；配合"前摇期间站定、锁定方向"才有读感） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	float MeleeWindup = 1.2f;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	float MeleeCooldown = 1.2f;

	// ── 陷阱 ──
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trap")
	float TrapLifetime = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trap")
	float TrapTriggerRadius = 150.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trap")
	float TrapMaxPlaceDistance = 1000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trap")
	float TrapFreezeDuration = 3.f;

	// ── 陨石落点 ──
	/** 黄圈预告/存在时长（11-01 用户改回 3s，三档阶梯 3.0 / 2.8 / 2.5） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Meteor")
	float MeteorWarningTime = 3.0f;

	/** AOE 半径 4m（07 C-3 已定） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Meteor")
	float MeteorRadius = 400.f;

	/** 一轮陨石生成几个黄圈（全屏同时出现，就近选一个弹） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Meteor")
	int32 MeteorCircleCount = 6;

	/**
	 * 圈与圈的最小间距（cm）—— 必须 ≥ 2 × MeteorRadius(400) = 800。
	 * 原值 600 会让相邻黄圈边缘重叠 200（2m）：视觉上连成一坨、判定时玩家可能同时处于两个圈内。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Meteor")
	float MeteorCircleSpacing = 800.f;

	// ── 牵引（10-29 用户新增：把玩家拉过来 → 禁锢 → 5s 前摇逼弹反）──
	//   流程：锁定 0.8s（玩家脚下小紫圈）→ 拉拽 1.2s → **禁锢玩家** → 5s 前摇 → 横扫结算
	//   横扫**可弹反**：成功 +10 条，失败 20 伤；这 5s 里 Boss 处于施法中，不会放别的技能
	/** 抬手/锁定前摇 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pull")
	float PullWindup = 0.8f;

	/** 拉拽持续上限（到 3m 就提前结束） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pull")
	float PullDuration = 1.2f;

	/** 拉拽速度 7m/s > 玩家 4m/s：反向跑只能减速，跑不掉 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pull")
	float PullSpeed = 700.f;

	/** 拉到这个距离停下（cm），避免把玩家塞进 Boss 身体 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pull")
	float PullStopDistance = 300.f;

	/**
	 * 横扫前摇 5s（10-29 用户定）：这 5s 玩家被禁锢、不能移动，只能等最后 1s 窗口按 Q。
	 * 圈内的填充会随这 5s 涨满，等于一个不用读秒的倒计时。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pull")
	float PullSweepWindup = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pull")
	float PullSweepRadius = 350.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pull")
	float PullSweepDamage = 20.f;

	/** 弹反横扫的破防条增量（低于陨石 40：它比跑圈容易） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pull")
	float PullGaugeOnParry = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pull")
	float PullCooldown = 18.f;

	/** 只在"中距离"用：太近有近战，太远拉不到 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pull")
	float PullMinRange = 250.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pull")
	float PullMaxRange = 1400.f;

	// ── 火焰（10-29 用户新增：烧地逼走位）──
	// 形态：沿 **Boss 朝向** 铺一条长条（不是圆片）——长条只封一个方向，
	// 玩家必须**横向**跑出去才安全，这才是"走位"；圆片挪一步就躲掉了。
	/** 长条长度（cm）：从 Boss 身前 1m 起，向前铺 14m */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flame")
	float FlameLength = 1400.f;

	/** 长条宽度（cm）2.4m：玩家胶囊半径约 0.4m，横向跑 1.6m 即可脱出 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flame")
	float FlameWidth = 240.f;

	/** 长条起点离 Boss 的距离（cm），避免火焰盖住 Boss 自己 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flame")
	float FlameStartOffset = 100.f;

	/** 落地前的红圈预警 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flame")
	float FlameWarningTime = 1.0f;

	/** 火焰存在时长 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flame")
	float FlameDuration = 4.f;

	/** 每 0.5s 结算一次：站满 4s = 8 跳 × 3 = 24 伤 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flame")
	float FlameTickInterval = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flame")
	float FlameTickDamage = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flame")
	float FlameCooldown = 10.f;

	// ── Boss 血量阶段（02 §6.4 / 10 §第二优先）──
	// 100–60%：常态；≤40%：陨石加密；<20%：遁地解禁，三种技能全开（狂暴）
	/**
	 * 80% 阶段（10-30 补齐四档）：陨石**再加密一档**——7 圈 / 2.8s。
	 * 取的是 6→8、3.0→2.5 的中间值，不引入新概念，只是把"越来越挤"做成三段坡。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Meteor|Phase")
	float MeteorDense2Threshold = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Meteor|Phase")
	int32 MeteorDense2CircleCount = 7;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Meteor|Phase")
	float MeteorDense2WarningTime = 2.8f;

	/** 40% 阶段阈值（血量比例） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Meteor|Phase")
	float MeteorDenseThreshold = 0.4f;

	/** 40% 阶段陨石圈数 6 → 8（02 §6.4） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Meteor|Phase")
	int32 MeteorDenseCircleCount = 8;

	/** 40% 阶段：存在 3.0 → 2.5s */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Meteor|Phase")
	float MeteorDenseWarningTime = 2.5f;

	/** 20% 狂暴阈值：遁地不再"只触发一次"，进入常规冷却 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Burrow")
	float EnrageThreshold = 0.2f;

	/**
	 * 狂暴期遁地冷却（秒）。
	 * ⚠️ 文档只写"进入常规冷却"没给数字，暂定 15s（比陨石 12s 更长，因为 30 伤是最高的）。
	 * 手感不对就直接在 DA_Tuning_Standard 里改，不用重编译。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Burrow")
	float BurrowCooldown = 15.f;

	// ── 碎石柱（10-29 改：够高，跳不上去，只能按 F 交互上去躲全屏秒杀）──
	/** 每次陨石落地后生成的碎石柱数量 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Meteor|Rubble")
	int32 RubbleCount = 4;

	/**
	 * 柱子高度（cm）。跳跃上限 1.5m → 300~360 是**故意跳不上去**的，
	 * 唯一上去的方式是交互键 F（10-29 用户明确要求）。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Meteor|Rubble")
	float RubbleHeightMin = 300.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Meteor|Rubble")
	float RubbleHeightMax = 360.f;

	/** 底面边长（cm）：柱顶要站得下一个人，别低于 180 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Meteor|Rubble")
	float RubbleFootprintMin = 180.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Meteor|Rubble")
	float RubbleFootprintMax = 240.f;

	/** 生成环带：离玩家 5–25m 随机（太近会把人夹住，太远没有战术价值） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Meteor|Rubble")
	float RubbleMinDistance = 500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Meteor|Rubble")
	float RubbleMaxDistance = 2500.f;

	// ── 柱子交互（F 键）──
	/** 准星射线能选中柱子的最远距离（cm）——"对准就能交互"，不再额外卡距离 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interact")
	float InteractAimRange = 3000.f;

	/** 跳上/飞跃到另一根柱子的弧线时长（秒）：0.35s 足够看清是一跳，又不至于挨打 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interact")
	float ClimbDuration = 0.35f;

	/** 弧线抬升高度（cm）：直线插值太"电梯"，加个抛物线才像跳 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interact")
	float ClimbArcHeight = 120.f;

	// ── 全屏秒杀（10-29 新增：逼玩家上柱子）──
	/** 前摇 10s：屏幕上方醒目倒计时，玩家要在这段时间里跑到柱子前按 F 上去 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "InstaKill")
	float InstaKillCastTime = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "InstaKill")
	float InstaKillCooldown = 35.f;

	/** 伤害够大就是秒杀（走同一个伤害入口，死亡流程/结算都能复用） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "InstaKill")
	float InstaKillDamage = 9999.f;

	/** 场上至少要有一根柱子才允许放（一根都没有时放=必死，不公平） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "InstaKill")
	int32 InstaKillMinPillars = 1;

	// ── 冲撞破柱（10-30 改：锁定**玩家** 5s → 朝玩家直线冲；玩家只能躲到柱子后让它撞）──
	/**
	 * 锁定前摇 5s：这段时间 Boss 转身盯住玩家，屏幕有提示。
	 * 玩家跑不掉（冲刺 13m/s > 跑速 4m/s），唯一解法是让柱子挡在 Boss 和玩家之间。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge")
	float ChargeWindup = 5.0f;

	/** 冲撞速度 13m/s：比玩家 4m/s 快得多，但只走直线，横向躲得开 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge")
	float ChargeSpeed = 1300.f;

	/** 冲撞最长持续（防止冲出场外）：3s × 13m/s = 39m，足够追上任何跑位 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge")
	float ChargeMaxDuration = 3.0f;

	/** 撞到柱子的判定半径（cm） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge")
	float ChargeHitRadius = 220.f;

	/** 冲撞路径上撞到玩家的伤害 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge")
	float ChargePlayerDamage = 15.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge")
	float ChargeCooldown = 14.f;

	/** 撞掉几根柱子触发"接住"窗口（07 §3.3-D：3 根） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge")
	int32 ChargeBreakCount = 3;

	// ── 撞柱「接住」（07 §3.3-D，2026-11-01）──
	/** 每撞碎 1 根柱子加的破防条：34 × 3 = 102 → 封顶 100，撞满即破防 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge")
	float ChargeGaugePerPillar = 34.f;

	/** 撞满 3 根后的 Boss 僵直（破防前的那一下定格） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Charge")
	float ChargeBreakStagger = 0.8f;

	// ── 技能解锁节奏（07 §3.3-C，2026-11-01）──
	/** Tier1：追击累计 20s 或血量 ≤ 80% → 解锁牵引 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Tiers")
	float Tier1ChaseTime = 20.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Tiers")
	float Tier1HealthRatio = 0.8f;

	/** Tier2：血量 ≤ 60% → 解锁遁地 + 火焰 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Tiers")
	float Tier2HealthRatio = 0.6f;

	/** Tier3：血量 ≤ 40% → 解锁全屏秒杀 + 冲撞 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Tiers")
	float Tier3HealthRatio = 0.4f;

	/** 解锁瞬间 Boss 停顿（玩家读提示的空窗） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Tiers")
	float UnlockStagger = 1.5f;

	/** 屏幕中央技能名显示时长（07 §3.3-C：1s） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Tiers")
	float UnlockCueTime = 1.0f;

	/** 解锁后把其它技能冷却推后多久（避免两个新招同时来） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Tiers")
	float UnlockOtherSkillDelay = 2.f;

	// ── 感知（07 §3.1）──
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perception")
	float PerceptionRadius = 1200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perception")
	float PerceptionHalfAngle = 60.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perception")
	float AwarenessMemory = 4.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perception")
	float SuspicionConfirmTime = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perception")
	float BossChaseSpeed = 320.f;

	/** Boss 施法距离 10m（07 A-2：视野 12m > 施法 10m） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Perception")
	float BossCastRange = 1000.f;

	// ── Boss 与掩体交互（07 §3.3-B）──
	/** 撞碎碎石后的停顿（期间不移动、不攻击，可被玩家打） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss")
	float BossCoverBreakStagger = 0.5f;

	/** 卡住兜底：直线冲锋持续时间 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss")
	float BossStallChargeTime = 1.5f;

	// ── 场地参考点（10-29 用户决定：取消空气墙、场景可无限大）──
	// 这两个数**不再参与任何落点/放置判定**（陨石圈、陷阱都已去掉界内检查），
	// 只留着当"取景/刷新参考"，以及和文档数值对齐。改它们不会影响玩法。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena")
	FVector ArenaCenter = FVector(2000.f, 2000.f, 0.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena")
	float ArenaHalfSize = 2000.f;

	// ── 遁地（07 §3.3 / C-4：动作 0.8s + 出土前摇 1.5s）──
	/** 下沉动作时长（C-4 建议 0.8s） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Burrow")
	float BurrowSinkTime = 0.8f;

	/** 出土前摇（07 §3.3：1.5s） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Burrow")
	float BurrowWindup = 1.5f;

	/** 高伤（07 §3.3：30；原 40 → 10-28 难度下调） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Burrow")
	float BurrowDamage = 30.f;

	/** 出土命中半径 4m（07 §3.3 已定，与陨石同量级） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Burrow")
	float BurrowRadius = 400.f;

	// ── 角色 / Boss ──
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actor")
	float PlayerMaxHealth = 150.f;

	/** 4 m/s（07 §0） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actor")
	float PlayerWalkSpeed = 400.f;

	/** 常态普攻 5（07 最终方案；破防 ×12 后 = 60） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actor")
	float PlayerAttackDamage = 5.f;

	/** 普攻冷却 0.6s（破防 5s 内可打 6 刀，含跑位 1.25s） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actor")
	float PlayerAttackCooldown = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actor")
	float PlayerAttackRange = 200.f;

	// ── 伤害技能（07 §1.5 最终方案：平时 15，破防期 180） ──
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
	float PlayerSkillDamage = 15.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
	float PlayerSkillRange = 300.f;

	/** 前方锥形半角 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
	float PlayerSkillHalfAngle = 45.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
	float PlayerSkillCooldown = 8.f;

	/** 闪避（07 §0 / B-5） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actor")
	float DodgeDistance = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actor")
	float DodgeInvincibleTime = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actor")
	float DodgeCooldown = 0.8f;

	/** 跳跃：高 1.5m、滞空 0.6s（07 §0）。重力由这两个数反解，见 PlayerCharacter::BeginPlay */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actor")
	float JumpHeight = 150.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actor")
	float JumpAirTime = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actor")
	float BossMaxHealth = 950.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actor", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float BurrowThreshold = 0.6f;

	/** 全局单例式访问：资产路径固定，改路径只改这里 */
	static const UChuandiTuningData* Get();
};
