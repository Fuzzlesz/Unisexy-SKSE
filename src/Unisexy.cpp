#include "Unisexy.h"
#include "FormIDManager.h"
#include "PCH.h"
#include "Settings.h"

void Unisexy::DoSexyStuff()
{
	logger::info("Starting Unisexy head part processing...");
	auto startTime = std::chrono::high_resolution_clock::now();

	const auto& settings = *Settings::GetSingleton();
	auto& dataHandler = *RE::TESDataHandler::GetSingleton();
	const auto headFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSHeadPart>();

	if (!headFactory) {
		logger::error("Could not get BGSHeadPart factory. Aborting.");
		return;
	}

	int createdCount = 0;
	int processedCount = 0;
	int failedNoSourceFile = 0;
	int disabledVanillaCount = 0;  // Track disabled vanilla parts
	FormIDManager formIDManager;
	std::map<RE::BGSHeadPart::HeadPartType, std::pair<int, int>> skippedByType;  // Pair: male skips, female skips

	static const std::set<RE::BGSHeadPart::HeadPartType> loggableTypes = {
		RE::BGSHeadPart::HeadPartType::kHair,
		RE::BGSHeadPart::HeadPartType::kFacialHair,
		RE::BGSHeadPart::HeadPartType::kScar,
		RE::BGSHeadPart::HeadPartType::kEyebrows,
		RE::BGSHeadPart::HeadPartType::kMisc
	};

	std::set<std::string> existingEditorIDs;
	for (const auto& existingHeadPart : dataHandler.GetFormArray<RE::BGSHeadPart>()) {
		if (existingHeadPart && existingHeadPart->GetFormEditorID()) {
			existingEditorIDs.insert(existingHeadPart->GetFormEditorID());
		}
	}

	for (const auto& headPart : dataHandler.GetFormArray<RE::BGSHeadPart>()) {
		if (!headPart) {
			continue;
		}
		processedCount++;

		auto headPartType = static_cast<RE::BGSHeadPart::HeadPartType>(headPart->type.get());

		// Skip kMisc headparts without kExtraPart flag
		if (headPartType == RE::BGSHeadPart::HeadPartType::kMisc && !headPart->flags.all(RE::BGSHeadPart::Flag::kIsExtraPart)) {
			if (settings.IsVerboseLogging()) {
				logger::info("Skipping kMisc head part without kExtraPart flag: {} [{:08X}]",
					headPart->GetFormEditorID(), headPart->formID);
			}
			continue;
		}

		// Skip if the head part isn't playable
		if (!headPart->flags.all(RE::BGSHeadPart::Flag::kPlayable)) {
			if (settings.IsVerboseLogging()) {
				logger::info("Skipping non-playable head part: {} [{:08X}]",
					headPart->GetFormEditorID(), headPart->formID);
			}
			continue;
		}

		// Determine if the headpart should be processed based on gender settings
		using Flag = RE::BGSHeadPart::Flag;
		const bool isMale = headPart->flags.all(Flag::kMale);
		const bool isFemale = headPart->flags.all(Flag::kFemale);
		bool shouldProcess = false;

		if (isMale && !isFemale && settings.IsFemaleEnabled(headPartType)) {
			shouldProcess = true;  // Male part, female enabled
		} else if (!isMale && isFemale && settings.IsMaleEnabled(headPartType)) {
			shouldProcess = true;  // Female part, male enabled
		}

		if (!shouldProcess) {
			if (loggableTypes.contains(headPartType)) {
				if (isMale && !isFemale && !settings.IsFemaleEnabled(headPartType)) {
					skippedByType[headPartType].second++;  // Skipped due to female disabled
				} else if (!isMale && isFemale && !settings.IsMaleEnabled(headPartType)) {
					skippedByType[headPartType].first++;  // Skipped due to male disabled
				}
			}
			continue;
		}

		const char* editorID = headPart->GetFormEditorID();
		if (!editorID || editorID[0] == '\0') {
			logger::info("Skipping head part with no EditorID (FormID: {:08X})", headPart->GetFormID());
			continue;
		}

		std::string newEditorID = std::string(editorID) + "_Unisexy";
		if (existingEditorIDs.count(newEditorID) > 0) {
			if (settings.IsVerboseLogging()) {
				logger::info("Skipping duplicate head part: {}", newEditorID);
			}
			continue;  // Skip duplicates
		}

		auto* newHeadPart = headFactory->Create();
		if (!newHeadPart) {
			logger::error("Failed to create new head part instance for {}", newEditorID);
			continue;
		}

		newHeadPart->SetFormEditorID(newEditorID.c_str());

		// Copy properties from the original head part
		newHeadPart->flags = headPart->flags;
		newHeadPart->type = headPart->type;
		newHeadPart->extraParts = headPart->extraParts;
		newHeadPart->textureSet = headPart->textureSet;
		newHeadPart->color = headPart->color;
		newHeadPart->validRaces = headPart->validRaces;
		for (size_t i = 0; i < RE::BGSHeadPart::MorphIndices::kTotal; i++) {
			newHeadPart->morphs[i] = headPart->morphs[i];
		}

		// Flip gender flags based on settings
		if (isMale && !isFemale && settings.IsFemaleEnabled(headPartType)) {
			newHeadPart->flags.reset(Flag::kMale);
			newHeadPart->flags.set(Flag::kFemale);
		} else if (!isMale && isFemale && settings.IsMaleEnabled(headPartType)) {
			newHeadPart->flags.reset(Flag::kFemale);
			newHeadPart->flags.set(Flag::kMale);
		}

		const RE::TESFile* targetFile = GetFileFromFormID(headPart->formID);
		if (!targetFile) {
			failedNoSourceFile++;
			logger::error("No source file found for head part {} [{:08X}]. Skipping.",
				editorID, headPart->formID);
			delete newHeadPart;
			continue;
		}

		if (!formIDManager.AssignFormID(newHeadPart, targetFile)) {
			logger::error("Failed to assign FormID for {}", newEditorID);
			delete newHeadPart;
			continue;
		}

		dataHandler.AddFormToDataHandler(newHeadPart);
		existingEditorIDs.insert(newEditorID);
		createdCount++;

		if (settings.IsVerboseLogging()) {
			logger::info("Created new head part: {} [{:08X}] (Type: {}) from original {:08X}",
				newEditorID, newHeadPart->formID,
				Settings::GetHeadPartTypeName(headPartType),
				headPart->formID);
		}

		// Disable vanilla head part after successful creation if setting is enabled
		if (settings.IsDisableVanillaParts()) {
			headPart->flags.reset(RE::BGSHeadPart::Flag::kPlayable);
			disabledVanillaCount++;
			if (settings.IsVerboseLogging()) {
				logger::info("Disabled vanilla head part: {} [{:08X}] (Type: {})",
					headPart->GetFormEditorID(), headPart->formID,
					Settings::GetHeadPartTypeName(headPartType));
			}
		}
	}

	auto endTime = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
	logger::info("Finished processing in {} ms. Processed {} head parts, created {} new parts, disabled {} vanilla parts.",
		duration, processedCount, createdCount, disabledVanillaCount);

	if (failedNoSourceFile > 0) {
		logger::info("Failed to process {} head parts due to missing source files.", failedNoSourceFile);
	}

	bool loggedAnySkips = false;
	for (const auto& type : loggableTypes) {
		auto it = skippedByType.find(type);
		if (it != skippedByType.end() && (it->second.first > 0 || it->second.second > 0)) {
			if (!loggedAnySkips) {
				logger::info("Skipped head parts due to disabled types in Unisexy.ini:");
				loggedAnySkips = true;
			}
			if (it->second.first > 0) {
				logger::info("  {} (Male): {}", Settings::GetHeadPartTypeName(type), it->second.first);
			}
			if (it->second.second > 0) {
				logger::info("  {} (Female): {}", Settings::GetHeadPartTypeName(type), it->second.second);
			}
		}
	}
	if (!loggedAnySkips) {
		logger::info("No head parts were skipped due to disabled types.");
	}
}
