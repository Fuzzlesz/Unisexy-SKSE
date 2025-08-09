#include "Unisexy.h"
#include "FormIDManager.h"
#include "HeadPartUtils.h"
#include "PCH.h"
#include "Settings.h"

void Unisexy::DoSexyStuff()
{
	logger::info("Starting Unisexy head part processing...");
	const auto startTime = std::chrono::high_resolution_clock::now();

	const auto& settings = *Settings::GetSingleton();
	auto& dataHandler = *RE::TESDataHandler::GetSingleton();
	const auto headFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSHeadPart>();

	if (!headFactory) {
		logger::error("Could not get BGSHeadPart factory. Aborting process.");
		return;
	}

	// Processing counters
	int createdCount = 0;
	int processedCount = 0;
	int failedNoSourceFile = 0;
	int disabledOriginalCount = 0;
	FormIDManager formIDManager;

	// Track skipped parts by type and gender for summary reporting
	std::map<RE::BGSHeadPart::HeadPartType, std::pair<int, int>> skippedByType;  // male skips, female skips

	// Only log skips for these major types to avoid spam
	static const std::set<RE::BGSHeadPart::HeadPartType> reportableTypes = {
		RE::BGSHeadPart::HeadPartType::kHair,
		RE::BGSHeadPart::HeadPartType::kFacialHair,
		RE::BGSHeadPart::HeadPartType::kScar,
		RE::BGSHeadPart::HeadPartType::kEyebrows,
	};

	// Build set of existing EditorIDs to prevent duplicates
	std::set<std::string> existingEditorIDs;
	for (const auto& existingHeadPart : dataHandler.GetFormArray<RE::BGSHeadPart>()) {
		if (existingHeadPart && existingHeadPart->GetFormEditorID()) {
			existingEditorIDs.insert(existingHeadPart->GetFormEditorID());
		}
	}

	// Process each head part in the data handler
	for (const auto& headPart : dataHandler.GetFormArray<RE::BGSHeadPart>()) {
		if (!headPart) {
			continue;
		}
		processedCount++;

		const auto headPartType = static_cast<RE::BGSHeadPart::HeadPartType>(headPart->type.get());

		// Skip non-playable head parts
		if (!headPart->flags.all(RE::BGSHeadPart::Flag::kPlayable)) {
			if (settings.IsVerboseLogging()) {
				logger::info("Skipping non-playable head part: {} [{:08X}]",
					headPart->GetFormEditorID(), headPart->formID);
			}
			continue;
		}

		// Skip kMisc head parts as they are not player selectable
		if (headPartType == RE::BGSHeadPart::HeadPartType::kMisc) {
			if (settings.IsVerboseLogging()) {
				logger::info("Skipping kMisc head part: {} [{:08X}]",
					headPart->GetFormEditorID(), headPart->formID);
			}
			continue;
		}

		// Analyze gender flags
		using Flag = RE::BGSHeadPart::Flag;
		const bool isMale = headPart->flags.all(Flag::kMale);
		const bool isFemale = headPart->flags.all(Flag::kFemale);
		const bool isGenderless = !isMale && !isFemale;

		// Handle genderless/unisex head parts
		if (isGenderless) {
			if (settings.IsShowOnlyUnisexy()) {
				headPart->flags.reset(Flag::kPlayable);
				disabledOriginalCount++;
				if (settings.IsVerboseLogging()) {
					logger::info("Disabled genderless head part: {} [{:08X}] (Type: {})",
						headPart->GetFormEditorID(), headPart->formID,
						Settings::GetHeadPartTypeName(headPartType));
				}
			} else if (settings.IsVerboseLogging()) {
				logger::info("Skipping genderless head part: {} [{:08X}] (Type: {})",
					headPart->GetFormEditorID(), headPart->formID,
					Settings::GetHeadPartTypeName(headPartType));
			}
			continue;
		}

		// Determine if this head part should be processed and target gender
		bool shouldProcess = false;
		bool toFemale = false;

		if (isMale && !isFemale && settings.IsFemaleEnabled(headPartType)) {
			// Convert male part to female
			shouldProcess = true;
			toFemale = true;
		} else if (!isMale && isFemale && settings.IsMaleEnabled(headPartType)) {
			// Convert female part to male
			shouldProcess = true;
			toFemale = false;
		} else {
			// Track skipped parts for summary reporting
			if (reportableTypes.contains(headPartType)) {
				if (isMale && !isFemale && !settings.IsFemaleEnabled(headPartType)) {
					skippedByType[headPartType].second++;  // Female conversion disabled
				} else if (!isMale && isFemale && !settings.IsMaleEnabled(headPartType)) {
					skippedByType[headPartType].first++;  // Male conversion disabled
				}
			}
			if (settings.IsVerboseLogging()) {
				logger::info("Skipping head part: {} [{:08X}] (Type: {}) - conversion disabled",
					headPart->GetFormEditorID(), headPart->formID,
					Settings::GetHeadPartTypeName(headPartType));
			}
			continue;
		}

		// Generate EditorID for the new head part
		const std::string newEditorID = HeadPartUtils::GenerateUnisexyEditorID(headPart);
		if (newEditorID.empty()) {
			continue;
		}

		// Skip if we already created this head part
		if (existingEditorIDs.count(newEditorID) > 0) {
			if (settings.IsVerboseLogging()) {
				logger::info("Skipping duplicate head part: {}", newEditorID);
			}
			continue;
		}

		// Create the new gender-flipped head part
		auto* newHeadPart = HeadPartUtils::CreateUnisexyHeadPart(
			headFactory, headPart, newEditorID, toFemale, settings);
		if (!newHeadPart) {
			continue;
		}

		// Get source file for FormID assignment
		const RE::TESFile* targetFile = GetFileFromFormID(headPart->formID);
		if (!targetFile) {
			failedNoSourceFile++;
			logger::error("No source file found for head part {} [{:08X}]. Skipping.",
				headPart->GetFormEditorID(), headPart->formID);
			delete newHeadPart;
			continue;
		}

		// Assign FormID to the new head part
		if (!formIDManager.AssignFormID(newHeadPart, targetFile)) {
			logger::error("Failed to assign FormID for {}", newEditorID);
			delete newHeadPart;
			continue;
		}

		// Process extra parts for head part types that support them
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

		// Register the new head part with the data handler
		dataHandler.AddFormToDataHandler(newHeadPart);
		existingEditorIDs.insert(newEditorID);
		createdCount++;

		if (settings.IsVerboseLogging()) {
			logger::info("Created head part: {} [{:08X}] (Type: {}) from source [{:08X}]",
				newEditorID, newHeadPart->formID,
				Settings::GetHeadPartTypeName(headPartType),
				headPart->formID);
		}

		// Disable original head part if configured to show only Unisexy versions
		if (settings.IsShowOnlyUnisexy()) {
			headPart->flags.reset(Flag::kPlayable);
			disabledOriginalCount++;
			if (settings.IsVerboseLogging()) {
				logger::info("Disabled original head part: {} [{:08X}] (Type: {})",
					headPart->GetFormEditorID(), headPart->formID,
					Settings::GetHeadPartTypeName(headPartType));
			}
		}
	}

	// Calculate processing time and log summary
	const auto endTime = std::chrono::high_resolution_clock::now();
	const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
	logger::info("Processing completed in {} ms. Processed {} head parts, created {} new parts, disabled {} original parts.",
		duration, processedCount, createdCount, disabledOriginalCount);

	if (failedNoSourceFile > 0) {
		logger::info("Failed to process {} head parts due to missing source files.", failedNoSourceFile);
	}

	// Report skipped parts summary
	bool loggedAnySkips = false;
	for (const auto& type : reportableTypes) {
		const auto it = skippedByType.find(type);
		if (it != skippedByType.end() && (it->second.first > 0 || it->second.second > 0)) {
			if (!loggedAnySkips) {
				logger::info("Skipped head parts due to disabled conversion types in Unisexy.ini:");
				loggedAnySkips = true;
			}
			if (it->second.first > 0) {
				logger::info("  {} (Male conversion): {}", Settings::GetHeadPartTypeName(type), it->second.first);
			}
			if (it->second.second > 0) {
				logger::info("  {} (Female conversion): {}", Settings::GetHeadPartTypeName(type), it->second.second);
			}
		}
	}
	if (!loggedAnySkips) {
		logger::info("No head parts were skipped due to disabled conversion types.");
	}
}
