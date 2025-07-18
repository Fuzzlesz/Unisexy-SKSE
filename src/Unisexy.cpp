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
	int fallbackAttempted = 0;
	int fallbackSucceeded = 0;
	FormIDManager formIDManager;
	std::map<RE::BGSHeadPart::HeadPartType, int> skippedByType;

	// We want labelled types for logging
	static const std::set<RE::BGSHeadPart::HeadPartType> loggableTypes = {
		RE::BGSHeadPart::HeadPartType::kHair,
		RE::BGSHeadPart::HeadPartType::kFacialHair,
		RE::BGSHeadPart::HeadPartType::kScar,
		RE::BGSHeadPart::HeadPartType::kEyebrows
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

		// Skip if the head part type isn’t enabled in settings
		auto headPartType = static_cast<RE::BGSHeadPart::HeadPartType>(headPart->type.get());
		if (!settings.IsEnabled(headPartType)) {
			if (loggableTypes.contains(headPartType)) {
				skippedByType[headPartType]++;
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

		// Flip gender flags
		using Flag = RE::BGSHeadPart::Flag;
		auto& flags = newHeadPart->flags;
		const bool isMale = flags.all(Flag::kMale);
		const bool isFemale = flags.all(Flag::kFemale);
		if (isMale && !isFemale) {
			flags.reset(Flag::kMale);
			flags.set(Flag::kFemale);
		} else if (!isMale && isFemale) {
			flags.reset(Flag::kFemale);
			flags.set(Flag::kMale);
		}

		const RE::TESFile* targetFile = GetFileFromFormID(headPart->formID);
		if (!targetFile) {
			fallbackAttempted++;
			logger::warn("No source file found for head part {} [{:08X}]. Using fallback.",
				editorID, headPart->formID);
			targetFile = dataHandler.LookupModByName("Unisexy.esp");
			if (targetFile) {
				fallbackSucceeded++;
				logger::info("Using fallback plugin Unisexy.esp for head part {} [{:08X}]",
					editorID, headPart->formID);
			} else {
				logger::error("Fallback plugin Unisexy.esp not found. Skipping {}", newEditorID);
				delete newHeadPart;
				continue;
			}
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
	}

	auto endTime = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
	logger::info("Finished processing in {} ms. Processed {} head parts, created {} new parts.",
		duration, processedCount, createdCount);

	if (fallbackAttempted > 0) {
		logger::info("Fallback usage: Attempted {} times ({} succeeded, {} failed)",
			fallbackAttempted, fallbackSucceeded, fallbackAttempted - fallbackSucceeded);
	}

	bool loggedAnySkips = false;
	for (const auto& type : loggableTypes) {
		auto it = skippedByType.find(type);
		if (it != skippedByType.end() && it->second > 0) {
			if (!loggedAnySkips) {
				logger::info("Skipped head parts due to disabled types in Unisexy.ini:");
				loggedAnySkips = true;
			}
			logger::info("  {}: {}", Settings::GetHeadPartTypeName(type), it->second);
		}
	}
	if (!loggedAnySkips) {
		logger::info("No head parts were skipped due to disabled types.");
	}
}
