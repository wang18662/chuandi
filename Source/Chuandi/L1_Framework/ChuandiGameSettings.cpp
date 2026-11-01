#include "L1_Framework/ChuandiGameSettings.h"

const UChuandiGameSettings& UChuandiGameSettings::Get()
{
	const UChuandiGameSettings* Settings = GetDefault<UChuandiGameSettings>();
	check(Settings);
	return *Settings;
}
