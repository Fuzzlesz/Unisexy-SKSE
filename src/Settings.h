#pragma once

#include "RE/B/BGSHeadPart.h"
#include <ClibUtil/simpleIni.hpp>

class Settings : public clib_util::singleton::ISingleton<Settings>
{
public:
	// Load settings from INI file, handling legacy format migration
	void Load();

	// Check if male conversion is enabled for given head part type
	bool IsMaleEnabled(RE::BGSHeadPart::HeadPartType a_type) const;

	// Check if female conversion is enabled for given head part type
	bool IsFemaleEnabled(RE::BGSHeadPart::HeadPartType a_type) const;

	// Check if verbose logging is enabled
	bool IsVerboseLogging() const;

	// Check if only Unisexy parts should be shown (vanilla parts hidden)
	bool IsShowOnlyUnisexy() const;

	// Get human-readable name for head part type
	static std::string GetHeadPartTypeName(RE::BGSHeadPart::HeadPartType type);

private:
	// Gender-specific settings for each head part type
	struct GenderSettings
	{
		bool maleEnabled = false;    // Enable male conversion (female -> male)
		bool femaleEnabled = false;  // Enable female conversion (male -> female)
	};

	// Save configuration to INI file
	void SaveConfigFile(CSimpleIniA& ini, const std::string& iniPath);

	std::map<RE::BGSHeadPart::HeadPartType, GenderSettings> _enabledTypes;
	bool _verboseLogging = false;
	bool _showOnlyUnisexy = false;
};
