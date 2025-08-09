#include "HeadPartUtils.h"
#include "PCH.h"

namespace HeadPartUtils
{
	std::string GenerateUnisexyEditorID(const RE::BGSHeadPart* a_headPart)
	{
		// Source head part should never be null from loaded game data
		assert(a_headPart);

		const char* editorID = a_headPart->GetFormEditorID();
		// Some forms legitimately have no EditorID - this check is still needed
		if (!editorID || editorID[0] == '\0') {
			if (Settings::GetSingleton()->IsVerboseLogging()) {
				logger::debug("Head part [{:08X}] has no EditorID - skipping", a_headPart->GetFormID());
			}
			return "";
		}

		return std::string(editorID) + "_Unisexy";
	}

	RE::BGSHeadPart* CreateUnisexyHeadPart(
		RE::IFormFactory* a_factory,
		const RE::BGSHeadPart* a_sourcePart,
		const std::string& a_newEditorID,
		bool a_toFemale,
		const Settings& a_settings)
	{
		// Factory and source should never be null from validated game data
		assert(a_factory && a_sourcePart);

		// Create new head part - memory allocation can still fail
		auto* newHeadPart = static_cast<RE::BGSHeadPart*>(a_factory->Create());
		if (!newHeadPart) {
			logger::error("Failed to create head part instance for {} - memory allocation failed", a_newEditorID);
			return nullptr;
		}

		// Set the new EditorID
		newHeadPart->SetFormEditorID(a_newEditorID.c_str());

		// Copy all properties from source - preserves original design
		newHeadPart->flags = a_sourcePart->flags;
		newHeadPart->type = a_sourcePart->type;
		newHeadPart->extraParts = a_sourcePart->extraParts;
		newHeadPart->textureSet = a_sourcePart->textureSet;
		newHeadPart->color = a_sourcePart->color;
		newHeadPart->validRaces = a_sourcePart->validRaces;
		newHeadPart->model = a_sourcePart->model;

		// Copy morph data
		for (size_t i = 0; i < RE::BGSHeadPart::MorphIndices::kTotal; i++) {
			newHeadPart->morphs[i] = a_sourcePart->morphs[i];
		}

		// Apply gender flag changes
		using Flag = RE::BGSHeadPart::Flag;
		if (a_toFemale) {
			newHeadPart->flags.reset(Flag::kMale);
			newHeadPart->flags.set(Flag::kFemale);
		} else {
			newHeadPart->flags.reset(Flag::kFemale);
			newHeadPart->flags.set(Flag::kMale);
		}

		if (a_settings.IsVerboseLogging()) {
			logger::info("Created head part {} [{:08X}] (Type: {}) from source [{:08X}] (target: {})",
				a_newEditorID, newHeadPart->formID,
				Settings::GetHeadPartTypeName(a_sourcePart->type.get()),
				a_sourcePart->formID, a_toFemale ? "Female" : "Male");
		}

		return newHeadPart;
	}

	bool ProcessExtraParts(
		RE::BGSHeadPart* a_newHeadPart,
		const RE::BGSHeadPart* a_sourcePart,
		FormIDManager& a_formIDManager,
		const RE::TESFile* a_targetFile,
		std::set<std::string>& a_existingEditorIDs,
		const Settings& a_settings,
		int& a_createdCount)
	{
		// All parameters should be valid from caller
		assert(a_newHeadPart && a_sourcePart && a_targetFile);

		// Early exit if no extra parts to process
		if (a_sourcePart->extraParts.empty()) {
			if (a_settings.IsVerboseLogging()) {
				logger::debug("No extra parts to process for head part [{:08X}]", a_newHeadPart->formID);
			}
			return true;
		}

		// Get factory for creating extra parts
		const auto headFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSHeadPart>();
		assert(headFactory);  // Should always be available

		// Determine target gender from new head part's flags
		const bool targetIsFemale = a_newHeadPart->flags.all(RE::BGSHeadPart::Flag::kFemale);
		const bool targetIsMale = a_newHeadPart->flags.all(RE::BGSHeadPart::Flag::kMale);

		// This should never happen with properly created head parts
		assert(targetIsFemale != targetIsMale);  // Exactly one should be true

		RE::BSTArray<RE::BGSHeadPart*> newExtraParts;

		// Process each extra part
		for (const auto* extraPart : a_sourcePart->extraParts) {
			// Null extra parts shouldn't exist in loaded data, but handle gracefully
			if (!extraPart) {
				if (a_settings.IsVerboseLogging()) {
					logger::warn("Null extra part found in source head part [{:08X}]", a_sourcePart->formID);
				}
				continue;
			}

			// Analyze extra part gender compatibility
			const bool extraIsMale = extraPart->flags.all(RE::BGSHeadPart::Flag::kMale);
			const bool extraIsFemale = extraPart->flags.all(RE::BGSHeadPart::Flag::kFemale);
			const bool extraIsGenderless = !extraIsMale && !extraIsFemale;

			// Determine if gender flip is needed
			bool needsGenderFlip = false;
			if (extraIsGenderless) {
				// Genderless parts work with any target gender
				needsGenderFlip = false;
			} else if (extraIsMale && targetIsFemale) {
				needsGenderFlip = true;
			} else if (extraIsFemale && targetIsMale) {
				needsGenderFlip = true;
			}
			// If extra part already matches target gender, no flip needed

			if (!needsGenderFlip) {
				newExtraParts.push_back(const_cast<RE::BGSHeadPart*>(extraPart));
				if (a_settings.IsVerboseLogging()) {
					logger::debug("Using original extra part: {} [{:08X}]",
						extraPart->GetFormEditorID() ? extraPart->GetFormEditorID() : "NoEditorID",
						extraPart->formID);
				}
				continue;
			}

			// Generate EditorID for gender-flipped extra part
			const std::string baseEditorID = extraPart->GetFormEditorID() ?
			                                     std::string(extraPart->GetFormEditorID()) :
			                                     fmt::format("ExtraPart_{:08X}", extraPart->formID);
			const std::string newEditorID = baseEditorID + "_Unisexy";

			// Check if we already created this extra part
			if (a_existingEditorIDs.count(newEditorID) > 0) {
				// Search for existing version to reuse
				bool foundExisting = false;
				auto& dataHandler = *RE::TESDataHandler::GetSingleton();

				for (const auto& existingHeadPart : dataHandler.GetFormArray<RE::BGSHeadPart>()) {
					if (existingHeadPart && existingHeadPart->GetFormEditorID() &&
						std::string(existingHeadPart->GetFormEditorID()) == newEditorID) {
						newExtraParts.push_back(existingHeadPart);
						foundExisting = true;
						if (a_settings.IsVerboseLogging()) {
							logger::debug("Reusing existing extra part: {} [{:08X}]",
								newEditorID, existingHeadPart->formID);
						}
						break;
					}
				}

				if (!foundExisting) {
					// Fall back to original if we can't find the existing version
					newExtraParts.push_back(const_cast<RE::BGSHeadPart*>(extraPart));
					if (a_settings.IsVerboseLogging()) {
						logger::warn("Could not find existing extra part {}, using original [{:08X}]",
							newEditorID, extraPart->formID);
					}
				}
				continue;
			}

			// Create new gender-flipped extra part
			auto* newExtraPart = CreateUnisexyHeadPart(
				headFactory, extraPart, newEditorID, targetIsFemale, a_settings);

			if (!newExtraPart) {
				// Fall back to original extra part if creation fails
				newExtraParts.push_back(const_cast<RE::BGSHeadPart*>(extraPart));
				continue;
			}

			// Assign FormID and register with data handler
			if (!a_formIDManager.AssignFormID(newExtraPart, a_targetFile)) {
				logger::error("Failed to assign FormID for extra part {}", newEditorID);
				delete newExtraPart;
				newExtraParts.push_back(const_cast<RE::BGSHeadPart*>(extraPart));
				continue;
			}

			RE::TESDataHandler::GetSingleton()->AddFormToDataHandler(newExtraPart);
			a_existingEditorIDs.insert(newEditorID);
			newExtraParts.push_back(newExtraPart);
			a_createdCount++;

			if (a_settings.IsVerboseLogging()) {
				logger::info("Created extra part {} [{:08X}] for head part [{:08X}]",
					newEditorID, newExtraPart->formID, a_newHeadPart->formID);
			}
		}

		// Assign all processed extra parts to the new head part
		a_newHeadPart->extraParts = newExtraParts;

		if (a_settings.IsVerboseLogging() && !newExtraParts.empty()) {
			logger::info("Assigned {} extra parts to head part [{:08X}]",
				newExtraParts.size(), a_newHeadPart->formID);
		}

		return true;
	}
}
