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

		// Reserve space to avoid reallocation
		std::string result;
		result.reserve(strlen(editorID) + 8);  // "_Unisexy" is 8 chars
		result = editorID;
		result += "_Unisexy";
		return result;
	}

	RE::BGSHeadPart* CreateUnisexyHeadPart(
		RE::IFormFactory* a_factory,
		const RE::BGSHeadPart* a_sourcePart,
		const std::string& a_newEditorID,
		bool a_toFemale,
		[[maybe_unused]] const Settings& a_settings)
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

		// Copy all properties from source
		newHeadPart->flags = a_sourcePart->flags;
		newHeadPart->type = a_sourcePart->type;
		newHeadPart->extraParts = a_sourcePart->extraParts;
		newHeadPart->textureSet = a_sourcePart->textureSet;
		newHeadPart->color = a_sourcePart->color;
		newHeadPart->validRaces = a_sourcePart->validRaces;
		newHeadPart->model = a_sourcePart->model;

		// Copy morph data - use memcpy for better performance
		std::memcpy(newHeadPart->morphs, a_sourcePart->morphs,
			sizeof(a_sourcePart->morphs[0]) * RE::BGSHeadPart::MorphIndices::kTotal);

		// Apply gender flag changes
		using Flag = RE::BGSHeadPart::Flag;
		if (a_toFemale) {
			newHeadPart->flags.reset(Flag::kMale);
			newHeadPart->flags.set(Flag::kFemale);
		} else {
			newHeadPart->flags.reset(Flag::kFemale);
			newHeadPart->flags.set(Flag::kMale);
		}

		// Initialize the form
		newHeadPart->InitItem();

		return newHeadPart;
	}

	bool ProcessExtraParts(
		RE::BGSHeadPart* a_newHeadPart,
		const RE::BGSHeadPart* a_sourcePart,
		FormIDManager& a_formIDManager,
		const RE::TESFile* a_targetFile,
		std::set<std::string>& a_existingEditorIDs,
		const Settings& a_settings,
		int& a_createdCount,
		std::vector<std::tuple<std::string, std::uint32_t, std::uint32_t>>& a_conflictDetails)
	{
		// All parameters should be valid from caller
		assert(a_newHeadPart && a_sourcePart && a_targetFile);

		// Early exit if no extra parts to process
		const auto& extraParts = a_sourcePart->extraParts;
		if (extraParts.empty()) {
			if (a_settings.IsVerboseLogging()) {
				logger::debug("No extra parts to process for head part {} [{:08X}]",
					a_newHeadPart->GetFormEditorID() ? a_newHeadPart->GetFormEditorID() : "NoEditorID",
					a_newHeadPart->formID);
			}
			return true;
		}

		// Get factory for creating extra parts
		const auto headFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSHeadPart>();
		assert(headFactory);

		// Determine target gender from new head part's flags (cache the result)
		const bool targetIsFemale = a_newHeadPart->flags.all(RE::BGSHeadPart::Flag::kFemale);

		// Pre-allocate result array to avoid reallocations
		RE::BSTArray<RE::BGSHeadPart*> newExtraParts;
		newExtraParts.reserve(extraParts.size());

		// Cache verbose logging setting
		const bool verboseLogging = a_settings.IsVerboseLogging();

		// Cache data handler reference
		auto& dataHandler = *RE::TESDataHandler::GetSingleton();

		// Process each extra part
		if (verboseLogging) {
			logger::info("Processing {} extra parts for head part {} [{:08X}] -> {} [{:08X}]",
				extraParts.size(),
				a_sourcePart->GetFormEditorID() ? a_sourcePart->GetFormEditorID() : "NoEditorID",
				a_sourcePart->formID,
				a_newHeadPart->GetFormEditorID() ? a_newHeadPart->GetFormEditorID() : "NoEditorID",
				a_newHeadPart->formID);
		}

		for (const auto* extraPart : extraParts) {
			if (!extraPart) {
				if (verboseLogging) {
					logger::warn("Null extra part found in source head part {} [{:08X}]",
						a_sourcePart->GetFormEditorID() ? a_sourcePart->GetFormEditorID() : "NoEditorID",
						a_sourcePart->formID);
				}
				continue;
			}

			// Analyze extra part gender compatibility
			const auto extraFlags = extraPart->flags;
			const bool extraIsMale = extraFlags.all(RE::BGSHeadPart::Flag::kMale);
			const bool extraIsFemale = extraFlags.all(RE::BGSHeadPart::Flag::kFemale);
			const bool extraIsGenderless = !extraIsMale && !extraIsFemale;

			// Determine if gender flip is needed
			bool needsGenderFlip = false;

			if (!extraIsGenderless) {
				needsGenderFlip = (extraIsMale && targetIsFemale) || (extraIsFemale && !targetIsFemale);
			}

			if (!needsGenderFlip) {
				newExtraParts.push_back(const_cast<RE::BGSHeadPart*>(extraPart));
				if (verboseLogging) {
					logger::debug("Using original extra part: {} [{:08X}] (Type: {})",
						extraPart->GetFormEditorID() ? extraPart->GetFormEditorID() : "NoEditorID",
						extraPart->formID,
						Settings::GetHeadPartTypeName(static_cast<RE::BGSHeadPart::HeadPartType>(extraPart->type.get())));
				}
				continue;
			}

			// Generate EditorID for gender-flipped extra part
			const char* extraEditorID = extraPart->GetFormEditorID();
			std::string newEditorID;
			if (extraEditorID) {
				newEditorID.reserve(strlen(extraEditorID) + 8);
				newEditorID = extraEditorID;
				newEditorID += "_Unisexy";
			} else {
				newEditorID = fmt::format("ExtraPart_{:08X}_Unisexy", extraPart->formID);
			}

			// Check if we already created this extra part
			auto existingIt = a_existingEditorIDs.find(newEditorID);
			if (existingIt != a_existingEditorIDs.end()) {
				// Search for existing version to reuse
				bool foundExisting = false;
				for (const auto& existingHeadPart : dataHandler.GetFormArray<RE::BGSHeadPart>()) {
					if (existingHeadPart && existingHeadPart->GetFormEditorID()) {
						const char* existingID = existingHeadPart->GetFormEditorID();
						if (existingID && newEditorID == existingID) {
							newExtraParts.push_back(existingHeadPart);
							foundExisting = true;
							if (verboseLogging) {
								logger::info("Reusing existing extra part: {} [{:08X}] (Type: {}) for head part {} [{:08X}]",
									newEditorID,
									existingHeadPart->formID,
									Settings::GetHeadPartTypeName(static_cast<RE::BGSHeadPart::HeadPartType>(existingHeadPart->type.get())),
									a_newHeadPart->GetFormEditorID() ? a_newHeadPart->GetFormEditorID() : "NoEditorID",
									a_newHeadPart->formID);
							}
							break;
						}
					}
				}

				if (!foundExisting) {
					// Fall back to original if we can't find the existing version
					newExtraParts.push_back(const_cast<RE::BGSHeadPart*>(extraPart));
					if (verboseLogging) {
						logger::warn("Could not find existing extra part {}, using original {} [{:08X}]",
							newEditorID,
							extraPart->GetFormEditorID() ? extraPart->GetFormEditorID() : "NoEditorID",
							extraPart->formID);
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
				logger::error("Failed to create gender-flipped extra part for {} [{:08X}] (Source: {} [{:08X}])",
					extraPart->GetFormEditorID() ? extraPart->GetFormEditorID() : "NoEditorID",
					extraPart->formID,
					a_sourcePart->GetFormEditorID() ? a_sourcePart->GetFormEditorID() : "NoEditorID",
					a_sourcePart->formID);
				a_conflictDetails.emplace_back(newEditorID, 0, 0);  // Record failure
				continue;
			}

			// Assign FormID and register with data handler
			std::uint32_t conflictFormID = 0;

			if (!a_formIDManager.AssignFormID(newExtraPart, a_targetFile, conflictFormID)) {
				a_conflictDetails.emplace_back(newEditorID, conflictFormID, 0);
				logger::error("Failed to assign FormID for extra part {} (Source: {} [{:08X}])",
					newEditorID,
					a_sourcePart->GetFormEditorID() ? a_sourcePart->GetFormEditorID() : "NoEditorID",
					a_sourcePart->formID);
				delete newExtraPart;
				newExtraParts.push_back(const_cast<RE::BGSHeadPart*>(extraPart));
				continue;
			}

			// Store conflict details if there was a conflict
			if (conflictFormID != 0) {
				a_conflictDetails.emplace_back(newEditorID, conflictFormID, newExtraPart->formID);
			}

			// Set the file for the new extra part
			newExtraPart->SetFile(const_cast<RE::TESFile*>(a_targetFile));
			dataHandler.AddFormToDataHandler(newExtraPart);
			a_existingEditorIDs.insert(newEditorID);
			newExtraParts.push_back(newExtraPart);
			a_createdCount++;

			if (verboseLogging) {
				logger::info("Created extra part: {} [{:08X}] (Type: {}) for head part {} [{:08X}] (Source: {} [{:08X}])",
					newEditorID,
					newExtraPart->formID,
					Settings::GetHeadPartTypeName(static_cast<RE::BGSHeadPart::HeadPartType>(newExtraPart->type.get())),
					a_newHeadPart->GetFormEditorID() ? a_newHeadPart->GetFormEditorID() : "NoEditorID",
					a_newHeadPart->formID,
					a_sourcePart->GetFormEditorID() ? a_sourcePart->GetFormEditorID() : "NoEditorID",
					a_sourcePart->formID);
			}
		}

		// Assign all processed extra parts to the new head part
		a_newHeadPart->extraParts = std::move(newExtraParts);
		return true;
	}
}
