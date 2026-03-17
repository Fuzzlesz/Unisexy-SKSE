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
	// Track assigned FormIDs per plugin to prevent conflicts
	std::map<const RE::TESFile*, std::set<std::uint32_t>> assignedFormIDs_;
};
