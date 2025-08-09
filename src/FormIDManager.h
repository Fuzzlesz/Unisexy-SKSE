#pragma once

#include "RE/Skyrim.h"
#include <map>

class FormIDManager
{
public:
	FormIDManager();

	// Assign a unique FormID to the given form within the target plugin's namespace
	// Returns false if assignment fails due to conflicts or exhausted FormID range
	bool AssignFormID(RE::TESForm* form, const RE::TESFile* targetFile);

private:
	std::map<const RE::TESFile*, std::uint32_t> formCounts_;       // Current FormID counter per plugin
	std::map<const RE::TESFile*, std::uint32_t> formCountsTotal_;  // Total forms assigned per plugin (for tracking)
};

// Utility function to determine which plugin file a FormID belongs to
// Returns nullptr if the FormID doesn't correspond to a loaded plugin
const RE::TESFile* GetFileFromFormID(std::uint32_t formID);
