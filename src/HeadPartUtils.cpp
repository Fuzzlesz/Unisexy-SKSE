#include "HeadPartUtils.h"
#include "PCH.h"

namespace HeadPartUtils
{
	std::string GenerateUnisexyEditorID(const RE::BGSHeadPart* a_headPart)
	{
		if (!a_headPart) {
			logger::error("Cannot generate EditorID for null head part");
			return "";
		}

		const char* editorID = a_headPart->GetFormEditorID();
		if (!editorID || editorID[0] == '\0') {
			logger::info("Cannot generate EditorID for head part [{:08X}] - no EditorID present", a_headPart->GetFormID());
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
		// Validate required inputs
		if (!a_factory || !a_sourcePart) {
			logger::error("Invalid factory or source part for creating head part: {}", a_newEditorID);
			return nullptr;
		}

		// Create new head part instance
		auto* newHeadPart = static_cast<RE::BGSHeadPart*>(a_factory->Create());
		if (!newHeadPart) {
			logger::error("Failed to create new head part instance for {}", a_newEditorID);
			return nullptr;
		}

		// Set the new EditorID first
		newHeadPart->SetFormEditorID(a_newEditorID.c_str());

		// Copy all properties from source head part - this preserves the original design
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

		// Apply gender flag changes based on target gender
		using Flag = RE::BGSHeadPart::Flag;
		if (a_toFemale) {
			newHeadPart->flags.reset(Flag::kMale);
			newHeadPart->flags.set(Flag::kFemale);
			if (a_settings.IsVerboseLogging()) {
				logger::debug("Applied female flags for head part: {}", a_newEditorID);
			}
		} else {
			newHeadPart->flags.reset(Flag::kFemale);
			newHeadPart->flags.set(Flag::kMale);
			if (a_settings.IsVerboseLogging()) {
				logger::debug("Applied male flags for head part: {}", a_newEditorID);
			}
		}

		if (a_settings.IsVerboseLogging()) {
			logger::info("Created head part {} [{:08X}] (Type: {}) from source [{:08X}]",
				a_newEditorID, newHeadPart->formID,
				Settings::GetHeadPartTypeName(a_sourcePart->type.get()),
				a_sourcePart->formID);
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
		// Skip processing if no extra parts exist
		if (!a_newHeadPart || !a_sourcePart || a_sourcePart->extraParts.empty()) {
			if (a_settings.IsVerboseLogging()) {
				logger::debug("No extra parts to process for head part [{:08X}]",
					a_newHeadPart ? a_newHeadPart->formID : 0);
			}
			return true;
		}

		// Get factory for creating new extra parts
		auto* headFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSHeadPart>();
		if (!headFactory) {
			logger::error("Could not get BGSHeadPart factory for extra parts");
			return false;
		}

		// Determine target gender from the new head part's flags
		const bool newHeadPartIsFemale = a_newHeadPart->flags.all(RE::BGSHeadPart::Flag::kFemale);
		const bool newHeadPartIsMale = a_newHeadPart->flags.all(RE::BGSHeadPart::Flag::kMale);

		if (!newHeadPartIsFemale && !newHeadPartIsMale) {
			logger::error("New head part [{:08X}] has ambiguous gender flags for extra part processing",
				a_newHeadPart->formID);
			return false;
		}

		RE::BSTArray<RE::BGSHeadPart*> newExtraParts;

		// Process each extra part from the source
		for (const auto* extraPart : a_sourcePart->extraParts) {
			if (!extraPart) {
				if (a_settings.IsVerboseLogging()) {
					logger::warn("Found null extra part in source head part [{:08X}]", a_sourcePart->formID);
				}
				continue;
			}

			// Analyze extra part's gender compatibility
			const bool extraPartIsMale = extraPart->flags.all(RE::BGSHeadPart::Flag::kMale);
			const bool extraPartIsFemale = extraPart->flags.all(RE::BGSHeadPart::Flag::kFemale);
			const bool extraPartIsGenderless = !extraPartIsMale && !extraPartIsFemale;

			// Determine if we need to create a gender-flipped version
			bool needsGenderFlip = false;
			bool targetGenderIsFemale = false;

			if (extraPartIsGenderless) {
				// Genderless parts work with both genders
				needsGenderFlip = false;
				if (a_settings.IsVerboseLogging()) {
					logger::debug("Using genderless extra part as-is: {} [{:08X}]",
						extraPart->GetFormEditorID() ? extraPart->GetFormEditorID() : "NoEditorID",
						extraPart->formID);
				}
			} else if (extraPartIsMale && newHeadPartIsFemale) {
				// Need to create female version of male extra part
				needsGenderFlip = true;
				targetGenderIsFemale = true;
			} else if (extraPartIsFemale && newHeadPartIsMale) {
				// Need to create male version of female extra part
				needsGenderFlip = true;
				targetGenderIsFemale = false;
			} else {
				// Extra part already matches target gender
				needsGenderFlip = false;
			}

			if (!needsGenderFlip) {
				newExtraParts.push_back(const_cast<RE::BGSHeadPart*>(extraPart));
				if (a_settings.IsVerboseLogging()) {
					logger::info("Using original extra part: {} [{:08X}]",
						extraPart->GetFormEditorID() ? extraPart->GetFormEditorID() : "NoEditorID",
						extraPart->formID);
				}
				continue;
			}

			// Generate EditorID for the new gender-flipped extra part
			const std::string baseEditorID = extraPart->GetFormEditorID() ?
			                                     std::string(extraPart->GetFormEditorID()) :
			                                     fmt::format("ExtraPart_{:08X}", extraPart->formID);
			const std::string newEditorID = baseEditorID + "_Unisexy";

			if (newEditorID.empty()) {
				logger::error("Failed to generate EditorID for extra part of head part [{:08X}]",
					a_sourcePart->formID);
				newExtraParts.push_back(const_cast<RE::BGSHeadPart*>(extraPart));
				continue;
			}

			// Check if we already created this extra part
			if (a_existingEditorIDs.count(newEditorID) > 0) {
				if (a_settings.IsVerboseLogging()) {
					logger::info("Found duplicate extra part: {}, searching for existing version...", newEditorID);
				}

				bool foundExisting = false;
				auto& dataHandler = *RE::TESDataHandler::GetSingleton();

				// Search for existing version to reuse
				for (const auto& existingHeadPart : dataHandler.GetFormArray<RE::BGSHeadPart>()) {
					if (existingHeadPart && existingHeadPart->GetFormEditorID() &&
						std::string(existingHeadPart->GetFormEditorID()) == newEditorID) {
						newExtraParts.push_back(existingHeadPart);
						foundExisting = true;
						if (a_settings.IsVerboseLogging()) {
							logger::info("Reusing existing extra part: {} [{:08X}]",
								newEditorID, existingHeadPart->formID);
						}
						break;
					}
				}

				if (!foundExisting) {
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
				headFactory, extraPart, newEditorID, targetGenderIsFemale, a_settings);

			if (!newExtraPart) {
				logger::error("Failed to create extra part: {}", newEditorID);
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
				logger::info("Created extra part {} [{:08X}] for head part [{:08X}] (target gender: {})",
					newEditorID, newExtraPart->formID, a_newHeadPart->formID,
					targetGenderIsFemale ? "Female" : "Male");
			}
		}

		// Assign all processed extra parts to the new head part
		a_newHeadPart->extraParts = newExtraParts;

		if (a_settings.IsVerboseLogging()) {
			logger::info("Assigned {} extra parts to head part [{:08X}]",
				newExtraParts.size(), a_newHeadPart->formID);
			for (size_t i = 0; i < newExtraParts.size(); ++i) {
				const auto* part = newExtraParts[i];
				if (part) {
					logger::info("  Extra part {}: {} [{:08X}]", i,
						part->GetFormEditorID() ? part->GetFormEditorID() : "NoEditorID",
						part->formID);
				} else {
					logger::warn("  Extra part {}: NULL", i);
				}
			}
		}

		return true;
	}
}
