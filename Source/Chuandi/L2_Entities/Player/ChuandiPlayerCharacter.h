#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "ChuandiPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UChuandiAbilitySystemComponent;
class UChuandiAttributeSet;
class AChuandiCoverActor;
struct FInputActionValue;

/**
 * 玩家角色。D1 只做：移动 + 第三人称镜头 + ASC/属性集初始化。
 * 技能（普攻 / 弹反 / 布阱）在 D2–D5 挂上来。
 */
UCLASS()
class CHUANDI_API AChuandiPlayerCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AChuandiPlayerCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UChuandiAttributeSet* GetChuandiAttributeSet() const { return AttributeSet; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void OnAttackPressed();
	void OnParryPressed();
	void OnTrapPressed();
	void OnTrapReleased();
	/** 长按选位中按 X 取消（ChuandiTypes：Idle → Aiming →（松开释放 / X 取消）→ Idle） */
	void OnCancelPressed();

	/** 重开一局（调试用）：重载当前关卡 */
	void OnRestartPressed();
	void OnDodgePressed();
	void OnSkillPressed();

	/** F1：显示/隐藏 HUD 上的调试信息（状态机名、怀疑度条） */
	void OnToggleDebugHUDPressed();

	/** 长按选位期间每帧更新指示器（07 §2.2：期间可自由移动、不减速） */
	void UpdateTrapAim();

	// ── 柱子交互（10-29：柱子 3m+，跳不上去，只能按 F）──
	/** F：对准柱子 = 跳上去；站在柱子上对准另一根 = 飞跃过去 */
	void OnInteractPressed();

	/** 每帧更新准星锁定的柱子（HUD 用它决定要不要画 F 提示） */
	void UpdatePillarFocus();

	/** 跳上/飞跃的弧线推进 */
	void UpdateClimb(float DeltaSeconds);

	/** 准星求交：返回是否命中地面；OutValid 表示是否满足全部放置条件 */
	bool ComputeTrapAimPoint(FVector& OutPoint, bool& bOutValid) const;

public:
	/** 传递物（D30：开场直接给 3 个） */
	int32 GetTrapCharges() const { return TrapCharges; }

	/** 准星锁定的柱子（没有则 nullptr）——HUD 用它决定画不画 F 提示 */
	AChuandiCoverActor* GetFocusedPillar() const { return FocusedPillar.Get(); }

	/** 玩家脚下那根柱子（没站在柱子上则 nullptr） */
	AChuandiCoverActor* GetStandingPillar() const;

	/** 玩家是否站在柱子上：全屏秒杀技能的唯一保命判定 */
	bool IsStandingOnPillar() const;

	/** 正在跳上/飞跃到柱子（此期间忽略移动输入） */
	bool IsClimbing() const { return bClimbing; }

	// ── 录 demo 用的调试信息开关（F1）──
	/** HUD 是否显示状态机名/怀疑度这类开发信息；录视频时关掉 */
	bool IsDebugHUDVisible() const { return bShowDebugHUD; }
	void ToggleDebugHUD();

	/** 镜头轻推（11 表现层 §2.2：弹反成功要有"那一下"） */
	void PlayCameraKick(float Strength);

	/** 弹反定身（10-28 20:21：弹反期间不能移动），End 时恢复原移动模式 */
	void BeginParryRoot();
	void EndParryRoot();

	/** 弹反定身是否生效（HUD/输入可用它屏蔽移动） */
	bool IsParryRooted() const { return bParryRooted; }

	// ── 强制禁锢（10-29：牵引把玩家拉过来后定住，逼他弹反 5s 前摇的一击）──
	/** 与弹反定身分开记：弹反窗口结束时不能顺手把这个禁锢也解掉 */
	void BeginForceRoot();
	void EndForceRoot();
	bool IsForceRooted() const { return bForceRooted; }

	/** 消耗 1 个传递物，成功返回 true */
	bool ConsumeTrapCharge();

	/** 行动锁：弹反后摇（0.1s）/ 弹反失败硬直（0.3s）期间不能出招 */
	bool IsActionLocked() const;
	void LockActions(float Seconds);

	/** 闪避：0.3s 内位移 4m（07 §0） */
	void StartDash(const FVector& Direction, float Duration, float Speed, float InvincibleTime);
	bool IsDashing() const { return DashTimeLeft > 0.f; }

	/** 取走一次有效的布阱落点（由 GA_PlayerPlaceTrap 调用） */
	bool ConsumePendingTrapLocation(FVector& OutLocation);
	bool IsAimingTrap() const { return bAimingTrap; }

	/**
	 * 冷却剩余比例（1 = 刚进冷却，0 = 就绪），供 HUD 画"扫回"扇形。
	 * 08/09 UI 规范：环形冷却不能用 tag 判断，必须知道还剩多少。
	 */
	float GetParryCooldownRatio() const;
	float GetDodgeCooldownRatio() const;
	float GetSkillCooldownRatio() const;

	/** MoveSpeed 属性变化 → 同步到 CharacterMovement（减速 GE 生效的地方） */
	void OnMoveSpeedChanged(const struct FOnAttributeChangeData& Data);

	void HandleDeath(AActor* DeadActor);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UChuandiAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UChuandiAttributeSet> AttributeSet;

	// 输入资产按固定路径加载（纯 C++ 工程没有蓝图子类可以填引用）
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> AttackAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ParryAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> TrapAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DodgeAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SkillAction;

	/** 可布阱次数 */
	int32 TrapCharges = 3;

private:
	double ActionLockUntil = 0.0;

	FVector DashDirection = FVector::ForwardVector;
	float DashTimeLeft = 0.f;
	float DashSpeed = 0.f;

	// ── 布阱长按选位（07 §2.1）──
	bool bAimingTrap = false;
	FVector PendingTrapLocation = FVector::ZeroVector;
	bool bPendingTrapValid = false;
	float AimLogAccum = 0.f;

	// 冷却结束的世界时刻（HUD 扫回用）
	float ParryCooldownEndTime = 0.f;
	float DodgeCooldownEndTime = 0.f;
	float SkillCooldownEndTime = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<class ATrapIndicatorActor> TrapIndicator;

	// ── 视觉（04 §1/§2.1）：玩家 = Capsule 胶囊，天青 #6E9EB8 ──
	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** 头（方块机器人） */
	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	/** 面罩（金色，兼作朝向指示） */
	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TObjectPtr<UStaticMeshComponent> FacingMesh;

	/** 播放一次性动作动画（普攻/技能/闪避），结束后自动回 idle */
	void PlayActionAnim(class UAnimSequence* Seq, float Duration);


	void UpdateRobotAnim(float DeltaSeconds);

	/** 机器人模型（SkeletalMesh，从 ShooterAI 迁移）；为空则用几何体拼装 */
	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TObjectPtr<class USkeletalMeshComponent> RobotMesh;

	/** 接触阴影：脚下半透明黑圆片（10 交接 §5.5）。玩家是运行时生成的，只能在代码里建 */
	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TObjectPtr<UStaticMeshComponent> ShadowMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;


	// ── 几何体外观（11-01 改回几何体）──
	/** 8 个方块部件（BeginPlay 时才给它们建 MID —— 构造函数里建会导致关卡保存失败） */
	UPROPERTY(VisibleAnywhere, Category = "Chuandi")
	TArray<TObjectPtr<UStaticMeshComponent>> GeoParts;

	/** 每个部件的基准色（冷白青躯干 / 金色关节） */
	TArray<FLinearColor> GeoPartColors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> GeoMaterials;

	/** 在 BeginPlay 里给所有部件建 MID 并上基准色 */
	void BuildGeoMaterials();

	// ── 动画（ShooterAI 迁移的 Unarmed 序列，单节点播放，不需要 AnimBP）──
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> AnimIdle;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> AnimJog;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> AnimJump;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> AnimDash;
	UPROPERTY(Transient) TArray<TObjectPtr<class UAnimSequence>> AnimAttacks;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> CurrentAnim;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> ActionAnim;
	float ActionAnimTimer = 0.f;
	float CameraKickStrength = 0.f;   // 当前镜头后推量，Tick 里指数衰减
	bool bParryRooted = false;
	bool bForceRooted = false;
	EMovementMode SavedMoveMode = MOVE_Walking;

	// ── 柱子交互（10-29）──
	/** 准星当前锁定的柱子 */
	TWeakObjectPtr<AChuandiCoverActor> FocusedPillar;

	/** 跳上/飞跃：用 0.35s 的抛物线插值，不用 CharacterMovement */
	bool bClimbing = false;

	/** HUD 调试信息开关（默认开；录 demo 按 F1 关） */
	bool bShowDebugHUD = true;
	FVector ClimbStart = FVector::ZeroVector;
	FVector ClimbTarget = FVector::ZeroVector;
	float ClimbElapsed = 0.f;
};
