#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "ChuandiBossCharacter.generated.h"

class UStaticMeshComponent;
class UChuandiAbilitySystemComponent;
class UChuandiAttributeSet;
class UBossStateMachineComponent;
class UBossPerceptionComponent;
class UBreakMeterComponent;
class UMaterialInstanceDynamic;

/**
 * 主怪（D3 阶段）。
 * 用 ACharacter 而不是 AActor，是为了 D6 的追击能直接复用 CharacterMovement。
 * D3 里它不动，只站桩放声波。
 */
UCLASS()
class CHUANDI_API AChuandiBossCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AChuandiBossCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UChuandiAttributeSet* GetChuandiAttributeSet() const { return AttributeSet; }
	UBossStateMachineComponent* GetStateMachine() const { return StateMachine; }
	UBossPerceptionComponent* GetPerception() const { return Perception; }
	UBreakMeterComponent* GetBreakMeter() const { return BreakMeter; }

	/** 由状态机调用：轮换释放声波 / 陨石 / 近战（07 §3.3） */
	void TryCastNextSkill();
	void TryCastWave();
	/** 单独放陨石（12s 独立冷却，见 BossStateMachineComponent） */
	void TryCastMeteor();
	/** 贴身时优先近战：2m / 10 伤 / 0.4s 前摇 */
	void TryMelee();
	/** 遁地（血量 ≤60% 触发一次） */
	void TryBurrow();
	/** 牵引：把玩家拉过来，横扫逼弹反（10-29 用户需求） */
	void TryPull();
	/** 火焰：在玩家脚下与两侧铺 3 片火，逼走位（10-29 用户需求） */
	void TryFlame();
	/** 全屏秒杀：10s 前摇，玩家必须站在碎石柱上（10-29 用户需求） */
	void TryInstaKill();
	/** 冲撞碎石柱：撞毁 ChargeBreakCount 根 → 直接破防（10-29 用户需求） */
	void TryCharge();

	// ── 当前技能名（HUD 左侧栏读取，展示"现在 Boss 在放什么"）──
	void SetCurrentSkill(const FString& SkillName, float Duration = 3.f);
	FString GetCurrentSkillName() const;

	// ── 全屏秒杀的预警状态（HUD 读它画顶部倒计时）──
	void StartInstaKillWarning(float Duration);
	void ClearInstaKillWarning();
	bool IsInstaKillWarningActive() const;
	float GetInstaKillWarningRemaining() const;

	/** 冲撞撞毁一根柱子后调用：累计到阈值直接破防，返回是否触发了破防 */
	bool NotifyPillarDestroyedByCharge();

	// ── 技能解锁提示（07 §3.3-C）──
	void ShowUnlockCue(const FString& SkillName, float Duration);
	bool IsUnlockCueActive() const;
	FString GetUnlockCueText() const { return UnlockCueText; }
	float GetUnlockCueRemaining() const;

	// ── 冲撞锁定（10-30）：5s 前摇期间 HUD 显示"锁定你，去柱子后" ──
	void StartChargeLock(float Duration);
	void ClearChargeLock();
	bool IsChargeLockActive() const;
	float GetChargeLockRemaining() const;

	/** 让 Boss 停顿一段时间（撞柱后的硬直，FSM/移动都会读它） */
	void ApplyStagger(float Seconds);

	// ── 几何体外观（11-01 用户要求：Boss 也改回几何体）──
	/** 所有几何体部件（用于遁地隐藏/显示） */
	void SetBodyVisible(bool bVisible);
	/** 给所有部件上色 + 自发光（替代原来骨架网格上的单一 BodyMaterial） */
	void ApplyBodyColor(const FLinearColor& Color, float EmissiveStrength);

	/** 恢复每个部件自己的基准色（朱砂躯干 / 亮红头 / 金面罩） */
	void ApplyBaseColors();

	/**
	 * 即时闪色（08 §3：受击闪白 / 弹反成功闪金）。
	 * 优先级高于破防金脉冲，持续 Duration 秒后回到常态色。
	 */
	void FlashColor(const FLinearColor& Color, float Duration);

	/** 播一次性动作动画（近战/施法），时长 0 = 用动画自身长度 */
	void PlayActionAnim(class UAnimSequence* Seq, float Duration);
	/** 近战攻击动画（GA_BossMelee 起手时播） */
	class UAnimSequence* GetAttackAnim() const { return AnimAttack; }

	/**
	 * 撞碎碎石后的 0.5s 停顿（07 §3.3-B）：期间不移动、不攻击，但仍可被玩家攻击。
	 * 状态机在 Evaluate 里查它来决定要不要发起攻击。
	 */
	bool IsStaggered() const;

	/**
	 * 追击目标：状态机（5Hz）只负责"告诉它往哪走"，
	 * 真正每帧移动在 Tick 里做——否则 5Hz 的决策会把移动速度压成 1/12。
	 */
	void SetChaseTarget(const FVector& TargetLocation);
	void ClearChaseTarget();
	void StopMoving();

	/** 施法/近战前先转向目标：不转身就放技能，玩家看不到前摇方向，也看不见它面朝哪 */
	void FaceActor(const AActor* Target);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void MoveTowards(const FVector& TargetLocation, float DeltaSeconds);

	void HandleDeath(AActor* DeadActor);

	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TObjectPtr<UChuandiAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TObjectPtr<UChuandiAttributeSet> AttributeSet;

	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TObjectPtr<UBossStateMachineComponent> StateMachine;

	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TObjectPtr<UBossPerceptionComponent> Perception;

	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TObjectPtr<UBreakMeterComponent> BreakMeter;

	/** 破防高亮用（04 §2.1：破防 = #FFD447 亮金） */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;


	void UpdateRobotAnim(float DeltaSeconds);

	/** 机器人模型（SkeletalMesh，与玩家同一套）；为空则用几何体拼装 */
	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TObjectPtr<class USkeletalMeshComponent> BossRobotMesh;

	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> AnimIdle;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> AnimJog;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> AnimAttack;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> CurrentAnim;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> ActionAnim;
	float ActionAnimTimer = 0.f;

	/** 头（Sphere）：Boss 是人形轮廓，不是方块 */
	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TObjectPtr<UStaticMeshComponent> BossHeadMesh;

	/** 接触阴影：脚下半透明黑圆片（10 交接 §5.5） */
	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TObjectPtr<UStaticMeshComponent> ShadowMesh;

	/**
	 * 遁地半透明（04 §2.1）：从 /Game/Chuandi/Materials/M_BossTranslucent 加载。
	 * 资产不存在时降级为"隐藏网格 + 地面圈提示"（07 §2.7 允许的位置提示方式）。
	 */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TranslucentMaterial;

	/** 几何体部件与其材质实例（改回几何体后的颜色通道） */
	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TArray<TObjectPtr<UStaticMeshComponent>> GeoParts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> GeoMaterials;

	/** 每个部件的基准色（BeginPlay 时才用它生成 MID —— 构造函数里建 MID 会导致关卡无法保存） */
	TArray<FLinearColor> GeoPartColors;

	bool bBurrowVisualActive = false;

	float FlashTimer = 0.f;
	float FlashDuration = 0.1f;
	FLinearColor FlashColorValue = FLinearColor::White;

	// ── 07 §3.3-B：撞碎石停顿 + 卡住兜底 ──
	/** 停顿结束的世界时刻 */
	double StaggerUntil = 0.0;
	/** 卡住检测：上次采样位置与计时 */
	FVector StallSamplePos = FVector::ZeroVector;
	float StallSampleTimer = 0.f;
	float StallAccum = 0.f;
	/** 直线冲锋结束时刻（无视碰撞） */
	double ChargeUntil = 0.0;

	// ── 10-29 新增：全屏秒杀预警 + 冲撞破柱计数 ──
	/** 全屏秒杀预警截止时刻（0 = 没有预警） */
	double InstaKillWarningEnd = 0.0;
	/** 本轮的冲撞已撞毁几根柱子（到 ChargeBreakCount 清零并破防） */
	int32 PillarsSmashedByCharge = 0;
	/** 冲撞锁定截止时刻（0 = 没有锁定） */
	double ChargeLockEnd = 0.0;

	/** 技能解锁提示截止时刻 + 文案（HUD 读） */
	double UnlockCueEnd = 0.0;
	FString UnlockCueText;

	/** 当前正在施放的技能名（HUD 左侧栏读；带过期时间，自动清空） */
	FString CurrentSkillName;
	double CurrentSkillExpire = 0.0;


private:
	bool bLoggedMovement = false;

	bool bHasChaseTarget = false;
	FVector ChaseTarget = FVector::ZeroVector;
	float MoveLogAccum = 0.f;
	int32 SkillIndex = 0;
};
