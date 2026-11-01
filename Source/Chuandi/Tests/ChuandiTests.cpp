#include "Misc/AutomationTest.h"

#include "L0_Core/ChuandiCombatMath.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * V1 的自动化证明：窗口判定的边界必须逐个钉死。
 * 跑法：UnrealEditor-Cmd.exe Chuandi.uproject -ExecCmds="Automation RunTests Chuandi;Quit" -unattended -nullrhi
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChuandiParryWindowTest, "Chuandi.Combat.ParryWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FChuandiParryWindowTest::RunTest(const FString& Parameters)
{
	const double Start = 100.0;
	const double End = 100.35;

	TestFalse(TEXT("窗口前 1ms → 失败"), UChuandiCombatMath::IsParrySuccess(Start, End, Start - 0.001, false));
	TestTrue(TEXT("恰好等于 Start → 成功"), UChuandiCombatMath::IsParrySuccess(Start, End, Start, false));
	TestTrue(TEXT("窗口中点 → 成功"), UChuandiCombatMath::IsParrySuccess(Start, End, 100.175, false));
	TestTrue(TEXT("恰好等于 End → 成功"), UChuandiCombatMath::IsParrySuccess(Start, End, End, false));
	TestFalse(TEXT("窗口后 1ms → 失败"), UChuandiCombatMath::IsParrySuccess(Start, End, End + 0.001, false));
	TestFalse(TEXT("窗口内但已消耗 → 失败"), UChuandiCombatMath::IsParrySuccess(Start, End, 100.1, true));
	TestFalse(TEXT("零长窗口且时间不等 → 失败"), UChuandiCombatMath::IsParrySuccess(Start, Start, Start + 0.001, false));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChuandiDistPointToSegmentTest, "Chuandi.Combat.DistPointToSegment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FChuandiDistPointToSegmentTest::RunTest(const FString& Parameters)
{
	const FVector A(-100.f, 0.f, 0.f);
	const FVector B(100.f, 0.f, 0.f);

	TestEqual(TEXT("点在段上 → 0"), UChuandiCombatMath::DistPointToSegment(FVector(0.f, 0.f, 0.f), A, B), 0.f);
	TestEqual(TEXT("点在线段外侧 → 100"), UChuandiCombatMath::DistPointToSegment(FVector(0.f, 100.f, 0.f), A, B), 100.f);
	TestEqual(TEXT("端点外侧 → 100"), UChuandiCombatMath::DistPointToSegment(FVector(200.f, 0.f, 0.f), A, B), 100.f);
	TestEqual(TEXT("零长度段 → 点到点"), UChuandiCombatMath::DistPointToSegment(FVector(0.f, 0.f, 0.f), A, A), 100.f);

	TestTrue(TEXT("半径 150 内应命中"), UChuandiCombatMath::IsWithinSegmentRadius(FVector(0.f, 120.f, 0.f), A, B, 150.f));
	TestFalse(TEXT("半径 50 外不应命中"), UChuandiCombatMath::IsWithinSegmentRadius(FVector(0.f, 120.f, 0.f), A, B, 50.f));

	float T = -1.f;
	const bool bCross = UChuandiCombatMath::FindSegmentCircleCrossT(
		FVector(0.f, 0.f, 0.f), 50.f, FVector(-200.f, 0.f, 0.f), FVector(200.f, 0.f, 0.f), T);
	TestTrue(TEXT("穿过圆心应返回 true"), bCross);
	TestTrue(TEXT("穿越点约在 0.375 处"), FMath::IsNearlyEqual(T, 0.375f, 0.01f));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
