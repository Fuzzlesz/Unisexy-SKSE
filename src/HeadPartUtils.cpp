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
			logger::info("Cannot generate EditorID for head part [{:08X}] with no EditorID", a_headPart->GetFormID());
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
		// Validate inputs
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

		// Set the new EditorID
		newHeadPart->SetFormEditorID(a_newEditorID.c_str());

		// Copy all properties from source head part

		newHeadPart->flags = a_sourcePart->flags;
		newHeadPart->type = a_sourcePart->type;
		newHeadPart->extraParts = a_sourcePart->extraParts;
		newHeadPart->textureSet = a_sourcePart->textureSet;
		newHeadPart->color = a_sourcePart->color;
		newHeadPart->validRaces = a_sourcePart->validRaces;
		for (size_t i = 0; i < RE::BGSHeadPart::MorphIndices::kTotal; i++) {
			newHeadPart->morphs[i] = a_sourcePart->morphs[i];
		}

		newHeadPart->model = a_sourcePart->model;

		// Flip gender flags based on target gender
		using Flag = RE::BGSHeadPart::Flag;
		if (a_toFemale) {
			newHeadPart->flags.reset(Flag::kMale);
			newHeadPart->flags.set(Flag::kFemale);
			if (a_settings.IsVerboseLogging()) {
				logger::debug("Set female flags for head part: {}", a_newEditorID);
			}
		} else {
			newHeadPart->flags.reset(Flag::kFemale);
			newHeadPart->flags.set(Flag::kMale);
			if (a_settings.IsVerboseLogging()) {
				logger::debug("Set male flags for head part: {}", a_newEditorID);
			}
		}

		if (a_settings.IsVerboseLogging()) {
			logger::info("Created head part {} [{:08X}] (Type: {}) from original [{:08X}]",
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
		// Early exit if no extra parts to process
		if (!a_newHeadPart || !a_sourcePart || a_sourcePart->extraParts.empty()) {
			if (a_settings.IsVerboseLogging()) {
				logger::debug("No extra parts to process for head part [{:08X}]", a_newHeadPart ? a_newHeadPart->formID : 0);
			}
			return true;
		}

		// Get head part factory
		auto* headFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSHeadPart>();
		if (!headFactory) {
			logger::error("Could not get BGSHeadPart factory for extra parts");
			return false;
		}

		// Determine target gender from new head part's flags
		bool newHeadPartIsFemale = a_newHeadPart->flags.all(RE::BGSHeadPart::Flag::kFemale);
		bool newHeadPartIsMale = a_newHeadPart->flags.all(RE::BGSHeadPart::Flag::kMale);

		if (!newHeadPartIsFemale && !newHeadPartIsMale) {
			logger::error("New head part [{:08X}] has unclear gender flags for extra part processing", a_newHeadPart->formID);
			return false;
		}

		RE::BSTArray<RE::BGSHeadPart*> newExtraParts;

		// Process each extra part
		for (const auto* extraPart : a_sourcePart->extraParts) {
			if (!extraPart) {
				if (a_settings.IsVerboseLogging()) {
					logger::warn("Found null extra part in source head part [{:08X}]", a_sourcePart->formID);
				}
				continue;
			}

			// Check extra part's gender flags
			bool extraPartIsMale = extraPart->flags.all(RE::BGSHeadPart::Flag::kMale);
			bool extraPartIsFemale = extraPart->flags.all(RE::BGSHeadPart::Flag::kFemale);
			bool extraPartIsGenderless = !extraPartIsMale && !extraPartIsFemale;

			// Determine if we need to create a gender-flipped version
			bool needsGenderFlip = false;
			bool targetGenderIsFemale = false;

			if (extraPartIsGenderless) {
				// Genderless parts are compatible with both genders
				needsGenderFlip = false;
				if (a_settings.IsVerboseLogging()) {
					logger::debug("Using genderless extra part as-is: {} [{:08X}]",
						extraPart->GetFormEditorID() ? extraPart->GetFormEditorID() : "NoEditorID",
						extraPart->formID);
				}
			} else if (extraPartIsMale && newHeadPartIsFemale) {
				// Need female version of male extra part
				needsGenderFlip = true;
				targetGenderIsFemale = true;
			} else if (extraPartIsFemale && newHeadPartIsMale) {
				// Need male version of female extra part
				needsGenderFlip = true;
				targetGenderIsFemale = false;
			} else {
				// Extra part matches target gender
				needsGenderFlip = false;
			}

			if (!needsGenderFlip) {
				newExtraParts.push_back(const_cast<RE::BGSHeadPart*>(extraPart));
				if (a_settings.IsVerboseLogging()) {
					logger::info("✓ Using original extra part: {} [{:08X}]",
						extraPart->GetFormEditorID() ? extraPart->GetFormEditorID() : "NoEditorID",
						extraPart->formID);
				}
				continue;
			}

			// Generate new editor ID for the extra part
			std::string baseEditorID = extraPart->GetFormEditorID() ?
			                               std::string(extraPart->GetFormEditorID()) :
			                               fmt::format("ExtraPart_{:08X}", extraPart->formID);
			std::string newEditorID = baseEditorID + "_Unisexy";

			if (newEditorID.empty()) {
				logger::error("Failed to generate EditorID for extra part of head part [{:08X}]", a_sourcePart->formID);
				newExtraParts.push_back(const_cast<RE::BGSHeadPart*>(extraPart));
				continue;
			}

			// Check for existing version of this extra part
			if (a_existingEditorIDs.count(newEditorID) > 0) {
				if (a_settings.IsVerboseLogging()) {
					logger::info("Found duplicate extra part: {}, searching for existing...", newEditorID);
				}

				bool foundExisting = false;
				auto& dataHandler = *RE::TESDataHandler::GetSingleton();

				for (const auto& existingHeadPart : dataHandler.GetFormArray<RE::BGSHeadPart>()) {
					if (existingHeadPart && existingHeadPart->GetFormEditorID() &&
						std::string(existingHeadPart->GetFormEditorID()) == newEditorID) {
						newExtraParts.push_back(existingHeadPart);
						foundExisting = true;
						if (a_settings.IsVerboseLogging()) {
							logger::info("✓ Found and reusing existing extra part: {} [{:08X}]",
								newEditorID, existingHeadPart->formID);
						}
						break;
					}
				}

				if (!foundExisting) {
					newExtraParts.push_back(const_cast<RE::BGSHeadPart*>(extraPart));
					if (a_settings.IsVerboseLogging()) {
						logger::warn("✗ Could not find existing extra part {}, using original [{:08X}]",
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

			// Assign FormID and add to data handler
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
				logger::info("Created extra part {} [{:08X}] for head part [{:08X}] (gender: {})",
					newEditorID, newExtraPart->formID, a_newHeadPart->formID,
					targetGenderIsFemale ? "Female" : "Male");
			}
		}

		// Assign processed extra parts to the new head part
		a_newHeadPart->extraParts = newExtraParts;

		if (a_settings.IsVerboseLogging()) {
			logger::info("✓ Assigned {} extra parts to head part [{:08X}]",
				newExtraParts.size(), a_newHeadPart->formID);
			for (size_t i = 0; i < newExtraParts.size(); ++i) {
				auto* part = newExtraParts[i];
				if (part) {
					logger::info("  Extra part {}: {} [{:08X}]", i,
						part->GetFormEditorID() ? part->GetFormEditorID() : "NoEditorID",
						part->formID);
				} else {
					logger::warn("  Extra part {}: NULL!", i);
				}
			}
		}

		return true;
	}
}
