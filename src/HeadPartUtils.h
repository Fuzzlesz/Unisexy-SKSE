#pragma once

#include "FormIDManager.h"
#include "RE/B/BGSHeadPart.h"
#include "RE/Skyrim.h"
#include "Settings.h"

namespace HeadPartUtils
{
	// Generate a Unisexy EditorID for the given head part
	// Returns empty string if the head part has no EditorID
	std::string GenerateUnisexyEditorID(const RE::BGSHeadPart* a_headPart);

	// Workaround for SetFile which was removed in CommonLibSSE-NG
	inline void SetFormFile(RE::TESForm* a_form, RE::TESFile* a_file)
	{
		if (!a_form || !a_file) return;
		if (!a_form->sourceFiles.array) {
			a_form->sourceFiles.array = RE::malloc<RE::TESFileArray>(sizeof(RE::TESFileArray));
			if (a_form->sourceFiles.array) {
				auto data = RE::malloc<RE::TESFile*>(sizeof(RE::TESFile*));
				if (data) {
					data[0] = a_file;
					struct ArrayLayout { RE::TESFile** data; std::uint32_t size; };
					auto layout = reinterpret_cast<ArrayLayout*>(a_form->sourceFiles.array);
					layout->data = data;
					layout->size = 1;
				}
			}
		}
	}

	// Create a gender-flipped copy of the source head part
	// Returns nullptr only if memory allocation fails
	RE::BGSHeadPart* CreateUnisexyHeadPart(
		RE::IFormFactory* a_factory,
		const RE::BGSHeadPart* a_sourcePart,
		const std::string& a_newEditorID,
		bool a_toFemale,
		const Settings& a_settings);

	// Process and create gender-flipped versions of extra parts
	// Updates a_createdCount with number of extra parts created
	// Appends FormID conflict details to a_conflictDetails
	bool ProcessExtraParts(
		RE::BGSHeadPart* a_newHeadPart,
		const RE::BGSHeadPart* a_sourcePart,
		FormIDManager& a_formIDManager,
		const RE::TESFile* a_targetFile,
		std::set<std::string>& a_existingEditorIDs,
		const Settings& a_settings,
		int& a_createdCount,
		std::vector<std::tuple<std::string, std::uint32_t, std::uint32_t>>& a_conflictDetails);
}
