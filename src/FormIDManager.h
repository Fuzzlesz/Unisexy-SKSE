#pragma once

#include "RE/Skyrim.h"

class FormIDManager
{
public:
	FormIDManager();

	// Assign a unique FormID to the given form within the target plugin's namespace
	// Returns false if assignment fails due to conflicts or invalid inputs
	// Sets outConflictFormID to the conflicting FormID if a conflict occurs
	bool AssignFormID(RE::TESForm* form, const RE::TESFile* targetFile, std::uint32_t& outConflictFormID);

private:
	// Current FormID counter per plugin
	std::map<const RE::TESFile*, std::uint32_t> formCounts_;
	// Track assigned FormIDs per plugin to prevent conflicts
	std::map<const RE::TESFile*, std::set<std::uint32_t>> assignedFormIDs_;
};

// Utility function to determine which plugin file a FormID belongs to
// Returns nullptr if the FormID doesn't correspond to a loaded plugin
const RE::TESFile* GetFileFromFormID(std::uint32_t formID);
