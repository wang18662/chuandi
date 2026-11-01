#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ChuandiHUD.generated.h"

/**
 * 零资产 HUD。
 *
 * 弹反相关的三条反馈全部在这里，且只"读"状态、不订阅事件：
 *   1. 窗口开启 → 屏幕下方青色条（教玩家什么时候按）
 *   2. Boss 进入破防的上升沿 → 中央 "PARRY!" 大字
 *   3. 玩家掉血的下降沿 → 屏幕边缘泛红
 * 这样 L4 不依赖 L1/L2 的任何回调，砍模块也不会伤 UI。
 */
UCLASS()
class CHUANDI_API AChuandiHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawHealthBarFor(AActor* Actor, float X, float Y, float Width, float Height,
		const FLinearColor& Fill, const FString& Label);

	// 上一帧状态，用来取"上升沿/下降沿"
	bool  bWasBossBroken = false;
	float LastPlayerHealth = -1.f;

	/** Boss 血量阶段（1 / 2=40% 陨石加密 / 3=20% 狂暴），用于阶段切换提示 */
	int32 LastBossPhase = 1;
	float PhaseAnnounceRemaining = 0.f;
	FString PhaseAnnounceText;

	float ParryFlashRemaining = 0.f;
	float HitFlashRemaining = 0.f;

	// ── 04_视觉规范 §4 动效 ──
	/** 弹反成功：全屏白闪 0.1s（07 §1.4） */
	float WhiteFlashRemaining = 0.f;
	/** 弹反失败：屏幕暗角 0.3s（04 §4） */
	float VignetteRemaining = 0.f;
	/** 记录上一次的冷却状态，用于"恢复时白闪一次"（07 §1.5） */
	bool bWasParryCooling = false;
	bool bWasDodgeCooling = false;
	bool bWasSkillCooling = false;
	float ParryReadyFlash = 0.f;
	float DodgeReadyFlash = 0.f;
	float SkillReadyFlash = 0.f;

	// ── 09_UI 规范 / CHANGELOG 16:04-16:08：右下角技能栏 ──
	void DrawDisc(float CX, float CY, float Radius, const FLinearColor& Color, int32 Segments = 24);
	void DrawRing(float CX, float CY, float Radius, float Thickness, const FLinearColor& Color,
		int32 Segments = 24);
	void DrawDiamond(float CX, float CY, float Size, const FLinearColor& Color);
	void DrawSector(float CX, float CY, float Radius, float StartDeg, float EndDeg,
		const FLinearColor& Color, int32 Segments = 16);
	void DrawSlotLabel(float CX, float BottomY, const FString& Text, bool bDim);
	void DrawSkillBar(AActor* PlayerPawn, float ScreenW, float ScreenH, double Now);
};
