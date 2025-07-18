#pragma once

#include "RE/Skyrim.h"
#include <unordered_map>

class FormIDManager
{
public:
	FormIDManager();

	bool AssignFormID(RE::TESForm* form, const RE::TESFile* targetFile);

private:
	std::unordered_map<const RE::TESFile*, std::uint32_t> formCounts_;
};

const RE::TESFile* GetFileFromFormID(std::uint32_t formID);
