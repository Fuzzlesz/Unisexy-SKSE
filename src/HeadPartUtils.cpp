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

		// Copy all properties from source - preserves original design
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
				const char* headPartEditorID = a_newHeadPart->GetFormEditorID() ?
				                                   a_newHeadPart->GetFormEditorID() :
				                                   "NoEditorID";
				logger::debug("No extra parts to process for head part {} [{:08X}]",
					headPartEditorID, a_newHeadPart->formID);
			}
			return true;
		}

		const bool verboseLogging = a_settings.IsVerboseLogging();
		const bool targetIsFemale = a_newHeadPart->flags.all(RE::BGSHeadPart::Flag::kFemale);
		const auto headFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSHeadPart>();
		if (!headFactory) {
			const char* headPartEditorID = a_newHeadPart->GetFormEditorID() ?
			                                   a_newHeadPart->GetFormEditorID() :
			                                   "NoEditorID";
			logger::error("Could not get BGSHeadPart factory for extra parts of head part {} [{:08X}]",
				headPartEditorID, a_newHeadPart->formID);
			return false;
		}

		RE::BSTArray<RE::BGSHeadPart*, RE::BSTArrayHeapAllocator> newExtraParts;
		newExtraParts.reserve(extraParts.size());

		for (const auto* extraPart : extraParts) {
			if (!extraPart) {
				newExtraParts.push_back(nullptr);
				continue;
			}

			const auto newEditorID = GenerateUnisexyEditorID(extraPart);
			if (newEditorID.empty()) {
				newExtraParts.push_back(const_cast<RE::BGSHeadPart*>(extraPart));
				continue;
			}

			// Check if EditorID already exists
			RE::BGSHeadPart* existingPart = nullptr;
			for (const auto* form : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::BGSHeadPart>()) {
				if (form && form->GetFormEditorID() && newEditorID == form->GetFormEditorID()) {
					existingPart = const_cast<RE::BGSHeadPart*>(form);  // Remove const for assignment
					break;
				}
			}

			if (existingPart) {
				newExtraParts.push_back(existingPart);
				if (verboseLogging) {
					const char* headPartEditorID = a_newHeadPart->GetFormEditorID() ?
					                                   a_newHeadPart->GetFormEditorID() :
					                                   "NoEditorID";
					logger::info("Using existing extra part {} [{:08X}] for head part {} [{:08X}]",
						newEditorID.c_str(), existingPart->formID, headPartEditorID, a_newHeadPart->formID);
				}
				continue;
			}

			// Create new gender-flipped extra part
			auto* newExtraPart = CreateUnisexyHeadPart(
				headFactory, extraPart, newEditorID, targetIsFemale, a_settings);

			if (!newExtraPart) {
				newExtraParts.push_back(const_cast<RE::BGSHeadPart*>(extraPart));
				logger::error("Failed to create gender-flipped extra part for {} [{:08X}] (Source: {} [{:08X}])",
					extraPart->GetFormEditorID() ? extraPart->GetFormEditorID() : "NoEditorID",
					extraPart->formID,
					a_sourcePart->GetFormEditorID() ? a_sourcePart->GetFormEditorID() : "NoEditorID",
					a_sourcePart->formID);
				continue;
			}

			// Assign FormID and register with data handler
			std::uint32_t conflictFormID = 0;
			if (!a_formIDManager.AssignFormID(newExtraPart, a_targetFile, conflictFormID)) {
				logger::error("Failed to assign FormID for extra part {} (Source: {} [{:08X}])",
					newEditorID.c_str(),
					a_sourcePart->GetFormEditorID() ? a_sourcePart->GetFormEditorID() : "NoEditorID",
					a_sourcePart->formID);
				a_conflictDetails.emplace_back(newEditorID, conflictFormID, 0);
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
			RE::TESDataHandler::GetSingleton()->AddFormToDataHandler(newExtraPart);
			a_existingEditorIDs.insert(newEditorID);
			newExtraParts.push_back(newExtraPart);
			a_createdCount++;

			if (verboseLogging) {
				const char* newPartEditorID = newExtraPart->GetFormEditorID() ?
				                                  newExtraPart->GetFormEditorID() :
				                                  "NoEditorID";
				const char* headPartEditorID = a_newHeadPart->GetFormEditorID() ?
				                                   a_newHeadPart->GetFormEditorID() :
				                                   "NoEditorID";
				const char* sourcePartEditorID = a_sourcePart->GetFormEditorID() ?
				                                     a_sourcePart->GetFormEditorID() :
				                                     "NoEditorID";
				logger::info("Created extra part: {} [{:08X}] (Type: {}) for head part {} [{:08X}] (Source: {} [{:08X}])",
					newPartEditorID,
					newExtraPart->formID,
					Settings::GetHeadPartTypeName(static_cast<RE::BGSHeadPart::HeadPartType>(newExtraPart->type.get())),
					headPartEditorID,
					a_newHeadPart->formID,
					sourcePartEditorID,
					a_sourcePart->formID);
			}
		}

		// Assign all processed extra parts to the new head part
		a_newHeadPart->extraParts = newExtraParts;

		if (verboseLogging) {
			const char* headPartEditorID = a_newHeadPart->GetFormEditorID() ?
			                                   a_newHeadPart->GetFormEditorID() :
			                                   "NoEditorID";
			logger::info("Completed processing extra parts for head part {} [{:08X}]. Final extra parts:",
				headPartEditorID, a_newHeadPart->formID);

			for (const auto* part : a_newHeadPart->extraParts) {
				if (part) {
					const char* partEditorID = part->GetFormEditorID() ? part->GetFormEditorID() : "NoEditorID";
					logger::info("  - {} [{:08X}] (Type: {})",
						partEditorID,
						part->formID,
						Settings::GetHeadPartTypeName(static_cast<RE::BGSHeadPart::HeadPartType>(part->type.get())));
				}
			}
		}

		return true;
	}
}
