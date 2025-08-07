#pragma once

#include "RE/Skyrim.h"
#include <ClibUtil/simpleIni.hpp>

class Settings : public clib_util::singleton::ISingleton<Settings>
{
public:
	Settings() = default;
	void Load();
	[[nodiscard]] bool IsMaleEnabled(RE::BGSHeadPart::HeadPartType a_type) const;
	[[nodiscard]] bool IsFemaleEnabled(RE::BGSHeadPart::HeadPartType a_type) const;
	[[nodiscard]] bool IsVerboseLogging() const;
	[[nodiscard]] bool IsShowOnlyUnisexy() const;
	[[nodiscard]] static std::string GetHeadPartTypeName(RE::BGSHeadPart::HeadPartType type);

private:
	struct HeadPartSettings
	{
		bool maleEnabled = true;
		bool femaleEnabled = true;
	};
	std::map<RE::BGSHeadPart::HeadPartType, HeadPartSettings> _enabledTypes;
	bool _verboseLogging = false;
	bool _showOnlyUnisexy = false;
};
