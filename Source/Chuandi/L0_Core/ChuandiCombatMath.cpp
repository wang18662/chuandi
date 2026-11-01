#include "L0_Core/ChuandiCombatMath.h"

bool UChuandiCombatMath::IsParrySuccess(double WindowStart, double WindowEnd, double HitTime, bool bConsumed)
{
	if (bConsumed)
	{
		return false;
	}

	// 闭区间：早 1ms、晚 1ms 都算失败，恰好等于边界算成功
	return HitTime >= WindowStart && HitTime <= WindowEnd;
}

float UChuandiCombatMath::DistPointToSegment(const FVector& Point, const FVector& SegStart, const FVector& SegEnd)
{
	const FVector Seg = SegEnd - SegStart;
	const double SegLenSq = Seg.SizeSquared();

	// 退化成一个点
	if (SegLenSq < UE_DOUBLE_SMALL_NUMBER)
	{
		return static_cast<float>(FVector::Dist(Point, SegStart));
	}

	const double T = FMath::Clamp(FVector::DotProduct(Point - SegStart, Seg) / SegLenSq, 0.0, 1.0);
	const FVector Closest = SegStart + Seg * T;
	return static_cast<float>(FVector::Dist(Point, Closest));
}

bool UChuandiCombatMath::IsWithinSegmentRadius(const FVector& Point, const FVector& SegStart, const FVector& SegEnd, float Radius)
{
	return DistPointToSegment(Point, SegStart, SegEnd) <= Radius;
}

bool UChuandiCombatMath::FindSegmentCircleCrossT(const FVector& Point, float Radius, const FVector& SegStart, const FVector& SegEnd, float& OutT)
{
	// 在水平面（XY）上求交：角色与陷阱都在地面上，Z 抖动不该影响触发
	const FVector2D P(Point.X, Point.Y);
	const FVector2D A(SegStart.X, SegStart.Y);
	const FVector2D B(SegEnd.X, SegEnd.Y);

	const FVector2D D = B - A;
	const double A2 = D.SizeSquared();
	if (A2 < UE_DOUBLE_SMALL_NUMBER)
	{
		OutT = 0.f;
		return FVector2D::Distance(P, A) <= Radius;
	}

	const FVector2D F = A - P;
	const double B2 = 2.0 * FVector2D::DotProduct(F, D);
	const double C2 = F.SizeSquared() - static_cast<double>(Radius) * Radius;
	const double Disc = B2 * B2 - 4.0 * A2 * C2;

	if (Disc < 0.0)
	{
		return false;
	}

	const double SqrtDisc = FMath::Sqrt(Disc);
	const double T1 = (-B2 - SqrtDisc) / (2.0 * A2);
	const double T2 = (-B2 + SqrtDisc) / (2.0 * A2);

	// 取"进入圆"的那一刻
	if (T1 >= 0.0 && T1 <= 1.0)
	{
		OutT = static_cast<float>(T1);
		return true;
	}
	if (T2 >= 0.0 && T2 <= 1.0)
	{
		OutT = static_cast<float>(T2);
		return true;
	}

	// 整段都在圆内（起点已在圈里）
	if (T1 < 0.0 && T2 > 1.0)
	{
		OutT = 0.f;
		return true;
	}

	return false;
}
