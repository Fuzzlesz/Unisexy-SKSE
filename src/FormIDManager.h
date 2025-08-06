#pragma once

#include "RE/Skyrim.h"
#include <map>

class FormIDManager
{
public:
	FormIDManager();
	bool AssignFormID(RE::TESForm* form, const RE::TESFile* targetFile);

private:
	std::map<const RE::TESFile*, std::uint32_t> formCounts_;       // Next FormID per plugin
	std::map<const RE::TESFile*, std::uint32_t> formCountsTotal_;  // Total forms assigned per plugin
};

const RE::TESFile* GetFileFromFormID(std::uint32_t formID);
