#include "Settings.h"
#include "PCH.h"

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

	bool needsUpdate = false;
	bool hasOldKeys = false;  // Declare hasOldKeys at function scope
	if (ini.LoadFile(iniPath.c_str()) >= SI_OK) {
		// Check for old format keys
		hasOldKeys = ini.KeyExists("HeadPartTypes", "Hair") ||
		             ini.KeyExists("HeadPartTypes", "Scars") ||
		             ini.KeyExists("HeadPartTypes", "Brows") ||
		             ini.KeyExists("HeadPartTypes", "FacialHair");

		needsUpdate = hasOldKeys;
	} else {
		needsUpdate = true;  // File doesn't exist, will generate
	}

	// Set defaults
	_enabledTypes[RE::BGSHeadPart::HeadPartType::kHair] = { true, true };
	_enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair] = { false, false };
	_enabledTypes[RE::BGSHeadPart::HeadPartType::kScar] = { false, false };
	_enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows] = { false, false };
	_enabledTypes[RE::BGSHeadPart::HeadPartType::kMisc] = { false, false };
	_verboseLogging = false;
	_disableVanillaParts = false;

	// Load settings from INI
	if (!needsUpdate) {
		const char* section = "HeadPartTypes";
		bool foundValue;

		_enabledTypes[RE::BGSHeadPart::HeadPartType::kHair].maleEnabled = ini.GetBoolValue(section, "HairMale", true, &foundValue);
		logger::info("Setting {}::HairMale = {} {}", section, _enabledTypes[RE::BGSHeadPart::HeadPartType::kHair].maleEnabled, foundValue ? "" : "(default)");
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kScar].maleEnabled = ini.GetBoolValue(section, "ScarsMale", false, &foundValue);
		logger::info("Setting {}::ScarsMale = {} {}", section, _enabledTypes[RE::BGSHeadPart::HeadPartType::kScar].maleEnabled, foundValue ? "" : "(default)");
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows].maleEnabled = ini.GetBoolValue(section, "BrowsMale", false, &foundValue);
		logger::info("Setting {}::BrowsMale = {} {}", section, _enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows].maleEnabled, foundValue ? "" : "(default)");
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair].maleEnabled = ini.GetBoolValue(section, "FacialHairMale", false, &foundValue);
		logger::info("Setting {}::FacialHairMale = {} {}", section, _enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair].maleEnabled, foundValue ? "" : "(default)");

		_enabledTypes[RE::BGSHeadPart::HeadPartType::kHair].femaleEnabled = ini.GetBoolValue(section, "HairFemale", true, &foundValue);
		logger::info("Setting {}::HairFemale = {} {}", section, _enabledTypes[RE::BGSHeadPart::HeadPartType::kHair].femaleEnabled, foundValue ? "" : "(default)");
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kScar].femaleEnabled = ini.GetBoolValue(section, "ScarsFemale", false, &foundValue);
		logger::info("Setting {}::ScarsFemale = {} {}", section, _enabledTypes[RE::BGSHeadPart::HeadPartType::kScar].femaleEnabled, foundValue ? "" : "(default)");
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows].femaleEnabled = ini.GetBoolValue(section, "BrowsFemale", false, &foundValue);
		logger::info("Setting {}::BrowsFemale = {} {}", section, _enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows].femaleEnabled, foundValue ? "" : "(default)");
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair].femaleEnabled = ini.GetBoolValue(section, "FacialHairFemale", false, &foundValue);
		logger::info("Setting {}::FacialHairFemale = {} {}", section, _enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair].femaleEnabled, foundValue ? "" : "(default)");

		
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kMisc].maleEnabled = true;    // Not user-configurable
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kMisc].femaleEnabled = true;  // Not user-configurable

		_verboseLogging = ini.GetBoolValue("Debug", "VerboseLogging", false, &foundValue);
		logger::info("Setting Debug::VerboseLogging = {} {}", _verboseLogging, foundValue ? "" : "(default)");
		_disableVanillaParts = ini.GetBoolValue("Debug", "DisableVanillaParts", false, &foundValue);
		logger::info("Setting Debug::DisableVanillaParts = {} {}", _disableVanillaParts, foundValue ? "" : "(default)");
	} 
	
	else if (hasOldKeys) {

		// Load old format keys and map to new format
		const char* section = "HeadPartTypes";
		bool foundValue;

		// Map old keys to both male and female settings
		bool hairValue = ini.GetBoolValue(section, "Hair", true, &foundValue);
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kHair].maleEnabled = hairValue;
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kHair].femaleEnabled = hairValue;
		logger::info("Mapping old {}::Hair = {} to HairMale and HairFemale", section, hairValue);

		bool scarsValue = ini.GetBoolValue(section, "Scars", false, &foundValue);
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kScar].maleEnabled = scarsValue;
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kScar].femaleEnabled = scarsValue;
		logger::info("Mapping old {}::Scars = {} to ScarsMale and ScarsFemale", section, scarsValue);

		bool browsValue = ini.GetBoolValue(section, "Brows", false, &foundValue);
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows].maleEnabled = browsValue;
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows].femaleEnabled = browsValue;
		logger::info("Mapping old {}::Brows = {} to BrowsMale and BrowsFemale", section, browsValue);

		bool facialHairValue = ini.GetBoolValue(section, "FacialHair", false, &foundValue);
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair].maleEnabled = facialHairValue;
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair].femaleEnabled = facialHairValue;
		logger::info("Mapping old {}::FacialHair = {} to FacialHairMale and FacialHairFemale", section, facialHairValue);

		_enabledTypes[RE::BGSHeadPart::HeadPartType::kMisc].maleEnabled = true;    // Not user-configurable
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kMisc].femaleEnabled = true;  // Not user-configurable

		// Debug settings remain unchanged, as they are not affected by old format
		_verboseLogging = ini.GetBoolValue("Debug", "VerboseLogging", false, &foundValue);
		logger::info("Setting Debug::VerboseLogging = {} {}", _verboseLogging, foundValue ? "" : "(default)");
		_disableVanillaParts = ini.GetBoolValue("Debug", "DisableVanillaParts", false, &foundValue);
		logger::info("Setting Debug::DisableVanillaParts = {} {}", _disableVanillaParts, foundValue ? "" : "(default)");
	}

	// Update INI if needed
	if (needsUpdate) {
		ini.Reset();

		ini.SetValue("", nullptr, nullptr, "; Unisexy.ini - Configuration for Unisexy SKSE mod\n; This mod creates gender-flipped versions of head parts (hair, facial hair, scars, eyebrows).\n; Set each option to true/false to enable/disable creating gender-flipped versions.");

		ini.SetValue("HeadPartTypes", "HairMale", _enabledTypes[RE::BGSHeadPart::HeadPartType::kHair].maleEnabled ? "true" : "false", "\n; Enable converting female parts to male");
		ini.SetValue("HeadPartTypes", "ScarsMale", _enabledTypes[RE::BGSHeadPart::HeadPartType::kScar].maleEnabled ? "true" : "false");
		ini.SetValue("HeadPartTypes", "BrowsMale", _enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows].maleEnabled ? "true" : "false");
		ini.SetValue("HeadPartTypes", "FacialHairMale", _enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair].maleEnabled ? "true" : "false");

		ini.SetValue("HeadPartTypes", "HairFemale", _enabledTypes[RE::BGSHeadPart::HeadPartType::kHair].femaleEnabled ? "true" : "false", "\n; Enable converting male parts to female");
		ini.SetValue("HeadPartTypes", "ScarsFemale", _enabledTypes[RE::BGSHeadPart::HeadPartType::kScar].femaleEnabled ? "true" : "false");
		ini.SetValue("HeadPartTypes", "BrowsFemale", _enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows].femaleEnabled ? "true" : "false");
		ini.SetValue("HeadPartTypes", "FacialHairFemale", _enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair].femaleEnabled ? "true" : "false");

		ini.SetValue("Debug", "VerboseLogging", _verboseLogging ? "true" : "false", "\n; Enable detailed logging for debugging");
		ini.SetValue("Debug", "DisableVanillaParts", _disableVanillaParts ? "true" : "false", "\n; Disable original vanilla head parts after creating gender-flipped versions");

		logger::info("Generating/Updating settings file at {}", iniPath);
		if (ini.SaveFile(iniPath.c_str()) < 0) {
			logger::error("Failed to save settings file '{}'", iniPath);
		} else {
			logger::info("Successfully generated/updated settings file '{}'", iniPath);
		}
	} else {
		logger::info("Settings loaded. No update needed.");
	}
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
