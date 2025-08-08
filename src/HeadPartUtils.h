#pragma once

#include "FormIDManager.h"
#include "RE/B/BGSHeadPart.h"
#include "RE/Skyrim.h"
#include "Settings.h"

namespace HeadPartUtils
{
	std::string GenerateUnisexyEditorID(const RE::BGSHeadPart* a_headPart);
	RE::BGSHeadPart* CreateUnisexyHeadPart(
		RE::IFormFactory* a_factory,
		const RE::BGSHeadPart* a_sourcePart,
		const std::string& a_newEditorID,
		bool a_toFemale,
		const Settings& a_settings);
	bool ProcessExtraParts(
		RE::BGSHeadPart* a_newHeadPart,
		const RE::BGSHeadPart* a_sourcePart,
		FormIDManager& a_formIDManager,
		const RE::TESFile* a_targetFile,
		std::set<std::string>& a_existingEditorIDs,
		const Settings& a_settings,
		int& a_createdCount);
}
