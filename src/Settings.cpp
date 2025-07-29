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
	case RE::BGSHeadPart::HeadPartType::kMisc:
		return "Misc";
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
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kHair] = { true, true };
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair] = { true, true };
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kScar] = { true, true };
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows] = { true, true };
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kMisc] = { true, true };
		_verboseLogging = false;
		_disableVanillaParts = false;  // Default to false
		return;
	}

	const char* section = "HeadPartTypes";
	_enabledTypes[RE::BGSHeadPart::HeadPartType::kHair].maleEnabled = ini.GetBoolValue(section, "HairMale", true);
	_enabledTypes[RE::BGSHeadPart::HeadPartType::kScar].maleEnabled = ini.GetBoolValue(section, "ScarsMale", true);
	_enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows].maleEnabled = ini.GetBoolValue(section, "BrowsMale", true);
	_enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair].maleEnabled = ini.GetBoolValue(section, "FacialHairMale", true);
	_enabledTypes[RE::BGSHeadPart::HeadPartType::kMisc].maleEnabled = ini.GetBoolValue(section, "MiscMale", true);

	_enabledTypes[RE::BGSHeadPart::HeadPartType::kHair].femaleEnabled = ini.GetBoolValue(section, "HairFemale", true);
	_enabledTypes[RE::BGSHeadPart::HeadPartType::kScar].femaleEnabled = ini.GetBoolValue(section, "ScarsFemale", true);
	_enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows].femaleEnabled = ini.GetBoolValue(section, "BrowsFemale", true);
	_enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair].femaleEnabled = ini.GetBoolValue(section, "FacialHairFemale", true);
	_enabledTypes[RE::BGSHeadPart::HeadPartType::kMisc].femaleEnabled = ini.GetBoolValue(section, "MiscFemale", true);

	_verboseLogging = ini.GetBoolValue("Debug", "VerboseLogging", false);
	_disableVanillaParts = ini.GetBoolValue("Debug", "DisableVanillaParts", false);  // Load new setting

	logger::info("Settings loaded.");
}

bool Settings::IsMaleEnabled(RE::BGSHeadPart::HeadPartType a_type) const
{
	const auto it = _enabledTypes.find(a_type);
	return it != _enabledTypes.end() ? it->second.maleEnabled : false;
}

bool Settings::IsFemaleEnabled(RE::BGSHeadPart::HeadPartType a_type) const
{
	const auto it = _enabledTypes.find(a_type);
	return it != _enabledTypes.end() ? it->second.femaleEnabled : false;
}

bool Settings::IsVerboseLogging() const
{
	return _verboseLogging;
}

bool Settings::IsDisableVanillaParts() const
{
	return _disableVanillaParts;
}
