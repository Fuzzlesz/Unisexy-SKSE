#include "Settings.h"
#include "PCH.h"

namespace
{
	constexpr bool INI_DEBUG_LOGGING = false;  // Set to false for production
}

void Settings::Load()
{
	CSimpleIniA ini;
	ini.SetUnicode();

	const auto iniPath = fmt::format("Data/SKSE/Plugins/{}.ini", Version::PROJECT);
	logger::info("Loading settings from {}", iniPath);

	bool needsUpdate = false;
	bool hasOldKeys = false;

	// Set default values - hair enabled by default, others disabled
	_enabledTypes[RE::BGSHeadPart::HeadPartType::kHair] = { true, true };
	_enabledTypes[RE::BGSHeadPart::HeadPartType::kMisc] = { true, true };  // Tied to hair
	_enabledTypes[RE::BGSHeadPart::HeadPartType::kScar] = { false, false };
	_enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows] = { false, false };
	_enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair] = { false, false };
	_verboseLogging = false;
	_showOnlyUnisexy = false;

	if (ini.LoadFile(iniPath.c_str()) >= SI_OK) {
		if (INI_DEBUG_LOGGING) {
			logger::info("INI file loaded successfully. Dumping contents:");
			CSimpleIniA::TNamesDepend sections;
			ini.GetAllSections(sections);
			for (const auto& section : sections) {
				logger::info("Section: {}", section.pItem);
				CSimpleIniA::TNamesDepend keys;
				ini.GetAllKeys(section.pItem, keys);
				for (const auto& key : keys) {
					logger::info("  Key: {} = {}", key.pItem, ini.GetValue(section.pItem, key.pItem, ""));
				}
			}
		}

		// Check for legacy keys that need migration
		hasOldKeys = ini.KeyExists("HeadPartTypes", "Hair") ||
		             ini.KeyExists("HeadPartTypes", "Scars") ||
		             ini.KeyExists("HeadPartTypes", "Brows") ||
		             ini.KeyExists("HeadPartTypes", "FacialHair") ||
		             ini.KeyExists("Debug", "DisableVanillaParts");

		// Check if new format keys are missing
		const bool missingNewKeys = !ini.KeyExists("HeadPartTypes", "HairMale") ||
		                            !ini.KeyExists("HeadPartTypes", "ScarsMale") ||
		                            !ini.KeyExists("HeadPartTypes", "BrowsMale") ||
		                            !ini.KeyExists("HeadPartTypes", "FacialHairMale") ||
		                            !ini.KeyExists("HeadPartTypes", "HairFemale") ||
		                            !ini.KeyExists("HeadPartTypes", "ScarsFemale") ||
		                            !ini.KeyExists("HeadPartTypes", "BrowsFemale") ||
		                            !ini.KeyExists("HeadPartTypes", "FacialHairFemale") ||
		                            !ini.KeyExists("Debug", "VerboseLogging") ||
		                            !ini.KeyExists("Debug", "ShowOnlyUnisexy");

		needsUpdate = hasOldKeys || missingNewKeys;

		// Load values from INI, preserving existing defaults
		const char* section = "HeadPartTypes";
		bool foundValue = false;

		// Migrate legacy keys to new format
		if (hasOldKeys) {
			if (INI_DEBUG_LOGGING) {
				logger::info("Migrating legacy keys to new format:");
			}

			// Migrate each legacy key to both male and female variants
			if (ini.KeyExists(section, "Hair")) {
				const bool value = ini.GetBoolValue(section, "Hair", true, &foundValue);
				_enabledTypes[RE::BGSHeadPart::HeadPartType::kHair] = { value, value };
				if (INI_DEBUG_LOGGING) {
					logger::info("  Migrated Hair={} to HairMale={}, HairFemale={}", value, value, value);
				}
			}

			if (ini.KeyExists(section, "Scars")) {
				const bool value = ini.GetBoolValue(section, "Scars", false, &foundValue);
				_enabledTypes[RE::BGSHeadPart::HeadPartType::kScar] = { value, value };
				if (INI_DEBUG_LOGGING) {
					logger::info("  Migrated Scars={} to ScarsMale={}, ScarsFemale={}", value, value, value);
				}
			}

			if (ini.KeyExists(section, "Brows")) {
				const bool value = ini.GetBoolValue(section, "Brows", false, &foundValue);
				_enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows] = { value, value };
				if (INI_DEBUG_LOGGING) {
					logger::info("  Migrated Brows={} to BrowsMale={}, BrowsFemale={}", value, value, value);
				}
			}

			if (ini.KeyExists(section, "FacialHair")) {
				const bool value = ini.GetBoolValue(section, "FacialHair", false, &foundValue);
				_enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair] = { value, value };
				if (INI_DEBUG_LOGGING) {
					logger::info("  Migrated FacialHair={} to FacialHairMale={}, FacialHairFemale={}", value, value, value);
				}
			}

			if (ini.KeyExists("Debug", "DisableVanillaParts")) {
				const bool value = ini.GetBoolValue("Debug", "DisableVanillaParts", false, &foundValue);
				_showOnlyUnisexy = value;
				if (INI_DEBUG_LOGGING) {
					logger::info("  Migrated DisableVanillaParts={} to ShowOnlyUnisexy={}", value, value);
				}
			}
		}

		// Load current format keys (these override any migrated values)
		if (ini.KeyExists(section, "HairMale")) {
			_enabledTypes[RE::BGSHeadPart::HeadPartType::kHair].maleEnabled =
				ini.GetBoolValue(section, "HairMale", true, &foundValue);
			if (INI_DEBUG_LOGGING && foundValue) {
				logger::info("  Loaded HairMale={}", _enabledTypes[RE::BGSHeadPart::HeadPartType::kHair].maleEnabled);
			}
		}

		if (ini.KeyExists(section, "HairFemale")) {
			_enabledTypes[RE::BGSHeadPart::HeadPartType::kHair].femaleEnabled =
				ini.GetBoolValue(section, "HairFemale", true, &foundValue);
			if (INI_DEBUG_LOGGING && foundValue) {
				logger::info("  Loaded HairFemale={}", _enabledTypes[RE::BGSHeadPart::HeadPartType::kHair].femaleEnabled);
			}
		}

		// Load remaining head part type settings
		if (ini.KeyExists(section, "ScarsMale")) {
			_enabledTypes[RE::BGSHeadPart::HeadPartType::kScar].maleEnabled =
				ini.GetBoolValue(section, "ScarsMale", false, &foundValue);
		}
		if (ini.KeyExists(section, "ScarsFemale")) {
			_enabledTypes[RE::BGSHeadPart::HeadPartType::kScar].femaleEnabled =
				ini.GetBoolValue(section, "ScarsFemale", false, &foundValue);
		}
		if (ini.KeyExists(section, "BrowsMale")) {
			_enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows].maleEnabled =
				ini.GetBoolValue(section, "BrowsMale", false, &foundValue);
		}
		if (ini.KeyExists(section, "BrowsFemale")) {
			_enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows].femaleEnabled =
				ini.GetBoolValue(section, "BrowsFemale", false, &foundValue);
		}
		if (ini.KeyExists(section, "FacialHairMale")) {
			_enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair].maleEnabled =
				ini.GetBoolValue(section, "FacialHairMale", false, &foundValue);
		}
		if (ini.KeyExists(section, "FacialHairFemale")) {
			_enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair].femaleEnabled =
				ini.GetBoolValue(section, "FacialHairFemale", false, &foundValue);
		}

		// Load debug settings
		if (ini.KeyExists("Debug", "VerboseLogging")) {
			_verboseLogging = ini.GetBoolValue("Debug", "VerboseLogging", false, &foundValue);
		}
		if (ini.KeyExists("Debug", "ShowOnlyUnisexy")) {
			_showOnlyUnisexy = ini.GetBoolValue("Debug", "ShowOnlyUnisexy", false, &foundValue);
		}

		// Misc type always follows hair settings
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kMisc] = {
			_enabledTypes[RE::BGSHeadPart::HeadPartType::kHair].maleEnabled,
			_enabledTypes[RE::BGSHeadPart::HeadPartType::kHair].femaleEnabled
		};

		if (INI_DEBUG_LOGGING) {
			logger::info("Final loaded settings:");
			logger::info("  Hair: Male={}, Female={}",
				_enabledTypes[RE::BGSHeadPart::HeadPartType::kHair].maleEnabled,
				_enabledTypes[RE::BGSHeadPart::HeadPartType::kHair].femaleEnabled);
			logger::info("  Scars: Male={}, Female={}",
				_enabledTypes[RE::BGSHeadPart::HeadPartType::kScar].maleEnabled,
				_enabledTypes[RE::BGSHeadPart::HeadPartType::kScar].femaleEnabled);
			logger::info("  Brows: Male={}, Female={}",
				_enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows].maleEnabled,
				_enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows].femaleEnabled);
			logger::info("  FacialHair: Male={}, Female={}",
				_enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair].maleEnabled,
				_enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair].femaleEnabled);
			logger::info("  Debug: VerboseLogging={}, ShowOnlyUnisexy={}",
				_verboseLogging, _showOnlyUnisexy);
		}
	} else {
		logger::error("Failed to load INI file '{}'. Creating new file with defaults.", iniPath);
		needsUpdate = true;
	}

	// Create or update INI file if needed
	if (needsUpdate) {
		SaveConfigFile(ini, iniPath);
	} else {
		logger::info("Settings loaded successfully. No update needed.");
	}
}

void Settings::SaveConfigFile(CSimpleIniA& ini, const std::string& iniPath)
{
	ini.Reset();

	// Add header comments to explain the configuration
	ini.SetValue("", nullptr, nullptr,
		"; Unisexy.ini - Configuration for Unisexy SKSE mod\n"
		"; This mod creates gender-flipped versions of head parts (hair, facial hair, scars, eyebrows).\n"
		"; Set each option to true/false to enable/disable creating gender-flipped versions.");

	// HeadPartTypes section - organize by conversion direction
	ini.SetValue("HeadPartTypes", "HairMale",
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kHair].maleEnabled ? "true" : "false",
		"\n; Enable converting female parts to male versions");
	ini.SetValue("HeadPartTypes", "ScarsMale",
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kScar].maleEnabled ? "true" : "false");
	ini.SetValue("HeadPartTypes", "BrowsMale",
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows].maleEnabled ? "true" : "false");
	ini.SetValue("HeadPartTypes", "FacialHairMale",
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair].maleEnabled ? "true" : "false");

	ini.SetValue("HeadPartTypes", "HairFemale",
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kHair].femaleEnabled ? "true" : "false",
		"\n; Enable converting male parts to female versions");
	ini.SetValue("HeadPartTypes", "ScarsFemale",
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kScar].femaleEnabled ? "true" : "false");
	ini.SetValue("HeadPartTypes", "BrowsFemale",
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows].femaleEnabled ? "true" : "false");
	ini.SetValue("HeadPartTypes", "FacialHairFemale",
		_enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair].femaleEnabled ? "true" : "false");

	// Debug section
	ini.SetValue("Debug", "VerboseLogging", _verboseLogging ? "true" : "false",
		"\n; Enable detailed logging for debugging");
	ini.SetValue("Debug", "ShowOnlyUnisexy", _showOnlyUnisexy ? "true" : "false",
		"; Hide vanilla head parts, showing only Unisexy-created versions");

	// Clean up legacy keys that might still exist
	ini.Delete("HeadPartTypes", "Hair");
	ini.Delete("HeadPartTypes", "Scars");
	ini.Delete("HeadPartTypes", "Brows");
	ini.Delete("HeadPartTypes", "FacialHair");
	ini.Delete("Debug", "DisableVanillaParts");

	logger::info("Saving updated settings to {}", iniPath);
	if (ini.SaveFile(iniPath.c_str()) < 0) {
		logger::error("Failed to save settings file '{}'. Check file permissions.", iniPath);
	} else {
		logger::info("Successfully saved settings file '{}'", iniPath);
	}
}

bool Settings::IsMaleEnabled(RE::BGSHeadPart::HeadPartType a_type) const
{
	const auto it = _enabledTypes.find(a_type);
	return it != _enabledTypes.end() && it->second.maleEnabled;
}

bool Settings::IsFemaleEnabled(RE::BGSHeadPart::HeadPartType a_type) const
{
	const auto it = _enabledTypes.find(a_type);
	return it != _enabledTypes.end() && it->second.femaleEnabled;
}

bool Settings::IsVerboseLogging() const
{
	return _verboseLogging;
}

bool Settings::IsShowOnlyUnisexy() const
{
	return _showOnlyUnisexy;
}

std::string Settings::GetHeadPartTypeName(RE::BGSHeadPart::HeadPartType type)
{
	switch (type) {
	case RE::BGSHeadPart::HeadPartType::kHair:
		return "Hair";
	case RE::BGSHeadPart::HeadPartType::kFacialHair:
		return "FacialHair";
	case RE::BGSHeadPart::HeadPartType::kScar:
		return "Scars";
	case RE::BGSHeadPart::HeadPartType::kEyebrows:
		return "Brows";
	case RE::BGSHeadPart::HeadPartType::kMisc:
		return "Misc";
	default:
		return "Unknown";
	}
}
