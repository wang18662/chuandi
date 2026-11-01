#include "L1_Framework/ChuandiTuningData.h"
#include "L0_Core/ChuandiLog.h"

namespace
{
	const TCHAR* TuningDataPath = TEXT("/Game/Chuandi/Data/DA_Tuning_Standard.DA_Tuning_Standard");
}

const UChuandiTuningData* UChuandiTuningData::Get()
{
	static TWeakObjectPtr<const UChuandiTuningData> Cached;
	if (Cached.IsValid())
	{
		return Cached.Get();
	}

	UChuandiTuningData* Loaded = Cast<UChuandiTuningData>(
		StaticLoadObject(UChuandiTuningData::StaticClass(), nullptr, TuningDataPath));

	if (!Loaded)
	{
		UE_LOG(LogChuandi, Error,
			TEXT("[Tuning] 找不到 %s。请在 Content/Chuandi/Data/ 下新建 DataAsset（类选 ChuandiTuningData），命名为 DA_Tuning_Standard。"),
			TuningDataPath);
		return nullptr;
	}

	Cached = Loaded;

	// 资产里的值优先于代码默认值，所以启动时把关键数打出来，避免"改了代码却没生效"
	UE_LOG(LogChuandi, Log, TEXT("[Tuning] ParryWindow=%.2f  ParryCooldown=%.2f  WaveCastTime=%.2f  WaveRadius=%.0f  TrapLifetime=%.1f"),
		Loaded->ParryWindow, Loaded->ParryCooldown, Loaded->WaveCastTime,
		Loaded->WaveRadius, Loaded->TrapLifetime);

	return Loaded;
}
