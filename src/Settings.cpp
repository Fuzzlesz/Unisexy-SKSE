#include "Settings.h"
#include "PCH.h"
#include <ClibUtil/simpleIni.hpp>

std::string Settings::GetHeadPartTypeName(RE::BGSHeadPart::HeadPartType type)
{
	switch (type) {
	case RE::BGSHeadPart::HeadPartType::kHair:
		return "Hair";
	case RE::BGSHeadPart::HeadPartType::kFacialHair:
		return "FacialHair";
	case RE::BGSHeadPart::HeadPartType::kScar:
		return "Scar";
	case RE::BGSHeadPart::HeadPartType::kEyebrows:
		return "Eyebrows";
	default:
		return "Unknown";
	}
}

void Settings::Load()
{
	CSimpleIniA ini;
	ini.SetUnicode();
	ini.SetMultiKey();

	const auto iniPath = fmt::format("Data/SKSE/Plugins/{}.ini", Version::PROJECT);

	logger::info("Loading settings from {}", iniPath);

	if (const auto rc = ini.LoadFile(iniPath.c_str()); rc < 0) {
		logger::warn("Could not load settings file, using default values.");
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kHair] = true;
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair] = true;
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kScar] = true;
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows] = true;
		_verboseLogging = false;
		return;
	}

	const char* section = "HeadPartTypes";
	_enabledTypes[RE::BGSHeadPart::HeadPartType::kHair] = ini.GetBoolValue(section, "Hair", true);
	_enabledTypes[RE::BGSHeadPart::HeadPartType::kScar] = ini.GetBoolValue(section, "Scars", true);
	_enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows] = ini.GetBoolValue(section, "Brows", true);
	_enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair] = ini.GetBoolValue(section, "FacialHair", true);
	_verboseLogging = ini.GetBoolValue("Debug", "VerboseLogging", false);

	logger::info("Settings loaded.");
}

bool Settings::IsEnabled(RE::BGSHeadPart::HeadPartType a_type) const
{
	const auto it = _enabledTypes.find(a_type);
	return it != _enabledTypes.end() ? it->second : false;
}

bool Settings::IsVerboseLogging() const
{
	return _verboseLogging;
}
