#include "Unisexy.h"
#include "FormIDManager.h"
#include "HeadPartUtils.h"
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
	int disabledOriginalCount = 0;
	FormIDManager formIDManager;
	std::map<RE::BGSHeadPart::HeadPartType, std::pair<int, int>> skippedByType;  // Pair: male skips, female skips

	static const std::set<RE::BGSHeadPart::HeadPartType> loggableTypes = {
		RE::BGSHeadPart::HeadPartType::kHair,
		RE::BGSHeadPart::HeadPartType::kFacialHair,
		RE::BGSHeadPart::HeadPartType::kScar,
		RE::BGSHeadPart::HeadPartType::kEyebrows,
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

		// Skip if the head part isn't playable
		if (!headPart->flags.all(RE::BGSHeadPart::Flag::kPlayable)) {
			if (settings.IsVerboseLogging()) {
				logger::info("Skipping non-playable head part: {} [{:08X}]",
					headPart->GetFormEditorID(), headPart->formID);
			}
			continue;
		}

		// Determine gender flags
		using Flag = RE::BGSHeadPart::Flag;
		const bool isMale = headPart->flags.all(Flag::kMale);
		const bool isFemale = headPart->flags.all(Flag::kFemale);
		const bool noFlag = !isMale && !isFemale;

		// Handle unflagged head parts
		if (noFlag) {
			if (settings.IsShowOnlyUnisexy()) {
				headPart->flags.reset(Flag::kPlayable);
				disabledOriginalCount++;
				if (settings.IsVerboseLogging()) {
					logger::info("Disabled unisex head part: {} [{:08X}] (Type: {})",
						headPart->GetFormEditorID(), headPart->formID,
						Settings::GetHeadPartTypeName(headPartType));
				}
			} else if (settings.IsVerboseLogging()) {
				logger::info("Skipping unisex head part: {} [{:08X}] (Type: {})",
					headPart->GetFormEditorID(), headPart->formID,
					Settings::GetHeadPartTypeName(headPartType));
			}
			continue;
		}

		// Determine if the head part should be processed
		bool shouldProcess = false;
		bool toFemale = false;

		if (isMale && !isFemale && settings.IsFemaleEnabled(headPartType)) {
			shouldProcess = true;
			toFemale = true;
		} else if (!isMale && isFemale && settings.IsMaleEnabled(headPartType)) {
			shouldProcess = true;
			toFemale = false;
		} else {
			if (loggableTypes.contains(headPartType)) {
				if (isMale && !isFemale && !settings.IsFemaleEnabled(headPartType)) {
					skippedByType[headPartType].second++;
				} else if (!isMale && isFemale && !settings.IsMaleEnabled(headPartType)) {
					skippedByType[headPartType].first++;
				}
			}
			if (settings.IsVerboseLogging()) {
				logger::info("Skipping head part: {} [{:08X}] (Type: {})",
					headPart->GetFormEditorID(), headPart->formID,
					Settings::GetHeadPartTypeName(headPartType));
			}
			continue;
		}

		std::string newEditorID = HeadPartUtils::GenerateUnisexyEditorID(headPart);
		if (newEditorID.empty()) {
			continue;
		}

		if (existingEditorIDs.count(newEditorID) > 0) {
			if (settings.IsVerboseLogging()) {
				logger::info("Skipping duplicate head part: {}", newEditorID);
			}
			continue;
		}

		auto* newHeadPart = HeadPartUtils::CreateUnisexyHeadPart(
			headFactory, headPart, newEditorID, toFemale, settings);
		if (!newHeadPart) {
			continue;
		}

		const RE::TESFile* targetFile = GetFileFromFormID(headPart->formID);
		if (!targetFile) {
			failedNoSourceFile++;
			logger::error("No source file found for head part {} [{:08X}]. Skipping.",
				headPart->GetFormEditorID(), headPart->formID);
			delete newHeadPart;
			continue;
		}

		if (!formIDManager.AssignFormID(newHeadPart, targetFile)) {
			logger::error("Failed to assign FormID for {}", newEditorID);
			delete newHeadPart;
			continue;
		}

		// Process extra parts for hair, beard, and brows
		if (headPartType == RE::BGSHeadPart::HeadPartType::kHair ||
			headPartType == RE::BGSHeadPart::HeadPartType::kFacialHair ||
			headPartType == RE::BGSHeadPart::HeadPartType::kEyebrows) {
			if (!HeadPartUtils::ProcessExtraParts(
					newHeadPart, headPart, formIDManager, targetFile,
					existingEditorIDs, settings, createdCount)) {
				logger::error("Failed to process extra parts for {}", newEditorID);
				delete newHeadPart;
				continue;
			}
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
		if (settings.IsShowOnlyUnisexy()) {
			headPart->flags.reset(RE::BGSHeadPart::Flag::kPlayable);
			disabledOriginalCount++;
			if (settings.IsVerboseLogging()) {
				logger::info("Disabled original head part: {} [{:08X}] (Type: {})",
					headPart->GetFormEditorID(), headPart->formID,
					Settings::GetHeadPartTypeName(headPartType));
			}
		}
	}

	auto endTime = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
	logger::info("Finished processing in {} ms. Processed {} head parts, created {} new parts, disabled {} original parts.",
		duration, processedCount, createdCount, disabledOriginalCount);

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
