#include "Settings.h"
#include "PCH.h"

namespace
{
	constexpr bool INI_DEBUG_LOGGING = true;
}

void Settings::Load()
{
	CSimpleIniA ini;
	ini.SetUnicode();

	const auto iniPath = fmt::format("Data/SKSE/Plugins/{}.ini", Version::PROJECT);
	logger::info("Loading settings from {}", iniPath);

	bool needsUpdate = false;
	bool hasOldKeys = false;

	// Set defaults first (these will be overwritten by loaded values if present)
	_enabledTypes[RE::BGSHeadPart::HeadPartType::kHair] = { true, true };
	_enabledTypes[RE::BGSHeadPart::HeadPartType::kMisc] = { true, true };  // Tied to kHair
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

		// Check for old format keys that need migration
		hasOldKeys = ini.KeyExists("HeadPartTypes", "Hair") ||
		             ini.KeyExists("HeadPartTypes", "Scars") ||
		             ini.KeyExists("HeadPartTypes", "Brows") ||
		             ini.KeyExists("HeadPartTypes", "FacialHair") ||
		             ini.KeyExists("Debug", "DisableVanillaParts");

		// Check for missing new format keys
		bool missingNewKeys = !ini.KeyExists("HeadPartTypes", "HairMale") ||
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

		// Load values from INI, preserving existing settings
		const char* section = "HeadPartTypes";
		bool foundValue;

		// Handle old key migration first
		if (hasOldKeys) {
			if (INI_DEBUG_LOGGING) {
				logger::info("Migrating old key values to new format:");
			}

			if (ini.KeyExists(section, "Hair")) {
				bool value = ini.GetBoolValue(section, "Hair", true, &foundValue);
				_enabledTypes[RE::BGSHeadPart::HeadPartType::kHair] = { value, value };
				if (INI_DEBUG_LOGGING) {
					logger::info("  Migrated Hair={} to HairMale={}, HairFemale={}", value, value, value);
				}
			}

			if (ini.KeyExists(section, "Scars")) {
				bool value = ini.GetBoolValue(section, "Scars", false, &foundValue);
				_enabledTypes[RE::BGSHeadPart::HeadPartType::kScar] = { value, value };
				if (INI_DEBUG_LOGGING) {
					logger::info("  Migrated Scars={} to ScarsMale={}, ScarsFemale={}", value, value, value);
				}
			}

			if (ini.KeyExists(section, "Brows")) {
				bool value = ini.GetBoolValue(section, "Brows", false, &foundValue);
				_enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows] = { value, value };
				if (INI_DEBUG_LOGGING) {
					logger::info("  Migrated Brows={} to BrowsMale={}, BrowsFemale={}", value, value, value);
				}
			}

			if (ini.KeyExists(section, "FacialHair")) {
				bool value = ini.GetBoolValue(section, "FacialHair", false, &foundValue);
				_enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair] = { value, value };
				if (INI_DEBUG_LOGGING) {
					logger::info("  Migrated FacialHair={} to FacialHairMale={}, FacialHairFemale={}", value, value, value);
				}
			}

			if (ini.KeyExists("Debug", "DisableVanillaParts")) {
				bool value = ini.GetBoolValue("Debug", "DisableVanillaParts", false, &foundValue);
				_showOnlyUnisexy = value;
				if (INI_DEBUG_LOGGING) {
					logger::info("  Migrated DisableVanillaParts={} to ShowOnlyUnisexy={}", value, value);
				}
			}
		}

		// Load new format keys (these will override any migrated values)
		if (ini.KeyExists(section, "HairMale")) {
			_enabledTypes[RE::BGSHeadPart::HeadPartType::kHair].maleEnabled = ini.GetBoolValue(section, "HairMale", true, &foundValue);
			if (INI_DEBUG_LOGGING && foundValue) {
				logger::info("  Loaded HairMale={}", _enabledTypes[RE::BGSHeadPart::HeadPartType::kHair].maleEnabled);
			}
		}

		if (ini.KeyExists(section, "HairFemale")) {
			_enabledTypes[RE::BGSHeadPart::HeadPartType::kHair].femaleEnabled = ini.GetBoolValue(section, "HairFemale", true, &foundValue);
			if (INI_DEBUG_LOGGING && foundValue) {
				logger::info("  Loaded HairFemale={}", _enabledTypes[RE::BGSHeadPart::HeadPartType::kHair].femaleEnabled);
			}
		}

		// Load remaining headpart types (same pattern as above)
		if (ini.KeyExists(section, "ScarsMale")) {
			_enabledTypes[RE::BGSHeadPart::HeadPartType::kScar].maleEnabled = ini.GetBoolValue(section, "ScarsMale", false, &foundValue);
		}
		if (ini.KeyExists(section, "ScarsFemale")) {
			_enabledTypes[RE::BGSHeadPart::HeadPartType::kScar].femaleEnabled = ini.GetBoolValue(section, "ScarsFemale", false, &foundValue);
		}
		if (ini.KeyExists(section, "BrowsMale")) {
			_enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows].maleEnabled = ini.GetBoolValue(section, "BrowsMale", false, &foundValue);
		}
		if (ini.KeyExists(section, "BrowsFemale")) {
			_enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows].femaleEnabled = ini.GetBoolValue(section, "BrowsFemale", false, &foundValue);
		}
		if (ini.KeyExists(section, "FacialHairMale")) {
			_enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair].maleEnabled = ini.GetBoolValue(section, "FacialHairMale", false, &foundValue);
		}
		if (ini.KeyExists(section, "FacialHairFemale")) {
			_enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair].femaleEnabled = ini.GetBoolValue(section, "FacialHairFemale", false, &foundValue);
		}

		// Load debug settings
		if (ini.KeyExists("Debug", "VerboseLogging")) {
			_verboseLogging = ini.GetBoolValue("Debug", "VerboseLogging", false, &foundValue);
		}
		if (ini.KeyExists("Debug", "ShowOnlyUnisexy")) {
			_showOnlyUnisexy = ini.GetBoolValue("Debug", "ShowOnlyUnisexy", false, &foundValue);
		}

		// Tie Misc to Hair (must be done after all hair settings are loaded)
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
		logger::error("Failed to load INI file '{}'. Generating new file.", iniPath);
		needsUpdate = true;
	}

	// Update INI if needed (missing keys or old format detected)
	if (needsUpdate) {
		ini.Reset();

		// Header comment
		ini.SetValue("", nullptr, nullptr,
			"; Unisexy.ini - Configuration for Unisexy SKSE mod\n"
			"; This mod creates gender-flipped versions of head parts (hair, facial hair, scars, eyebrows).\n"
			"; Set each option to true/false to enable/disable creating gender-flipped versions.");

		// HeadPartTypes section
		ini.SetValue("HeadPartTypes", "HairMale", _enabledTypes[RE::BGSHeadPart::HeadPartType::kHair].maleEnabled ? "true" : "false",
			"\n; Enable converting female parts to male");
		ini.SetValue("HeadPartTypes", "ScarsMale", _enabledTypes[RE::BGSHeadPart::HeadPartType::kScar].maleEnabled ? "true" : "false");
		ini.SetValue("HeadPartTypes", "BrowsMale", _enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows].maleEnabled ? "true" : "false");
		ini.SetValue("HeadPartTypes", "FacialHairMale", _enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair].maleEnabled ? "true" : "false");

		ini.SetValue("HeadPartTypes", "HairFemale", _enabledTypes[RE::BGSHeadPart::HeadPartType::kHair].femaleEnabled ? "true" : "false",
			"\n; Enable converting male parts to female");
		ini.SetValue("HeadPartTypes", "ScarsFemale", _enabledTypes[RE::BGSHeadPart::HeadPartType::kScar].femaleEnabled ? "true" : "false");
		ini.SetValue("HeadPartTypes", "BrowsFemale", _enabledTypes[RE::BGSHeadPart::HeadPartType::kEyebrows].femaleEnabled ? "true" : "false");
		ini.SetValue("HeadPartTypes", "FacialHairFemale", _enabledTypes[RE::BGSHeadPart::HeadPartType::kFacialHair].femaleEnabled ? "true" : "false");

		// Debug section
		ini.SetValue("Debug", "VerboseLogging", _verboseLogging ? "true" : "false",
			"\n; Enable detailed logging for debugging");
		ini.SetValue("Debug", "ShowOnlyUnisexy", _showOnlyUnisexy ? "true" : "false",
			"\n; Disable all headparts except those generated by Unisexy");

		// Remove any old keys that might still exist
		ini.Delete("HeadPartTypes", "Hair");
		ini.Delete("HeadPartTypes", "Scars");
		ini.Delete("HeadPartTypes", "Brows");
		ini.Delete("HeadPartTypes", "FacialHair");
		ini.Delete("Debug", "DisableVanillaParts");

		logger::info("Generating/Updating settings file at {}", iniPath);
		if (ini.SaveFile(iniPath.c_str()) < 0) {
			logger::error("Failed to save settings file '{}'. Check write permissions or file locks.", iniPath);
		} else {
			logger::info("Successfully generated/updated settings file '{}'", iniPath);
		}
	} else {
		logger::info("Settings loaded. No update needed.");
	}
}

bool Settings::IsMaleEnabled(RE::BGSHeadPart::HeadPartType a_type) const
{
	auto it = _enabledTypes.find(a_type);
	return it != _enabledTypes.end() && it->second.maleEnabled;
}

bool Settings::IsFemaleEnabled(RE::BGSHeadPart::HeadPartType a_type) const
{
	auto it = _enabledTypes.find(a_type);
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
