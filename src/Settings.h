#pragma once

#include "RE/Skyrim.h"

class Settings : public clib_util::singleton::ISingleton<Settings>
{
public:
	void Load();
	[[nodiscard]] bool IsEnabled(RE::BGSHeadPart::HeadPartType a_type) const;
	[[nodiscard]] bool IsVerboseLogging() const;
	[[nodiscard]] static std::string GetHeadPartTypeName(RE::BGSHeadPart::HeadPartType type);

private:
	std::map<RE::BGSHeadPart::HeadPartType, bool> _enabledTypes;
	bool _verboseLogging = false;
};
