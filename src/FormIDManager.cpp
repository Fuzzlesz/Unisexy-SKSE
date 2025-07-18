#include "FormIDManager.h"
#include "Settings.h"

constexpr std::uint32_t ESL_FORM_LIMIT = 2048;
constexpr std::uint32_t ESL_HIGH_START = 0xFFF;     // Highest FormID for ESL
constexpr std::uint32_t ESL_MIN = 0x800;            // Minimum FormID for ESL
constexpr std::uint32_t ESP_HIGH_START = 0xFFFFFF;  // Highest FormID for ESP/ESM
constexpr std::uint32_t ESP_MIN = 0x800;            // Minimum FormID for ESP/ESM
constexpr std::uint32_t MAX_FORMID_ATTEMPTS = 10;   // Max attempts to resolve conflicts

FormIDManager::FormIDManager() {}

bool FormIDManager::AssignFormID(RE::TESForm* form, const RE::TESFile* targetFile)
{
	if (!form || !targetFile) {
		logger::error("Invalid form or target file for FormID assignment.");
		return false;
	}

	if (Settings::GetSingleton()->IsVerboseLogging()) {
		logger::info("Assigning FormID for form [{:08X}] in plugin: {}",
			form->formID, targetFile->GetFilename());
	}

	std::uint32_t newFormID = 0;
	if (targetFile->IsLight()) {
		// ESL: Start from 0xFFF and decrement
		auto it = formCounts_.find(targetFile);
		if (it == formCounts_.end()) {
			formCounts_[targetFile] = ESL_HIGH_START;
		}

		std::uint32_t attemptCount = 0;
		while (attemptCount < MAX_FORMID_ATTEMPTS) {
			if (formCounts_[targetFile] < ESL_MIN) {
				logger::error("No free FormID in ESL range for plugin: {}", targetFile->GetFilename());
				return false;
			}
			newFormID = 0xFE000000 | (static_cast<std::uint32_t>(targetFile->smallFileCompileIndex) << 12) | formCounts_[targetFile];
			if (!RE::TESDataHandler::GetSingleton()->LookupForm(newFormID, targetFile->GetFilename())) {
				formCounts_[targetFile]--;
				break;
			}
			logger::warn("FormID {:08X} already in use in plugin: {}. Trying next.",
				newFormID, targetFile->GetFilename());
			formCounts_[targetFile]--;
			attemptCount++;
		}
		if (attemptCount >= MAX_FORMID_ATTEMPTS) {
			logger::error("Failed to find free FormID for plugin: {} after {} attempts.",
				targetFile->GetFilename(), MAX_FORMID_ATTEMPTS);
			return false;
		}
	} else {
		// ESP/ESM: Start from 0xFFFFFF and decrement
		auto it = formCounts_.find(targetFile);
		if (it == formCounts_.end()) {
			formCounts_[targetFile] = ESP_HIGH_START;
		}

		std::uint32_t attemptCount = 0;
		while (attemptCount < MAX_FORMID_ATTEMPTS) {
			if (formCounts_[targetFile] < ESP_MIN) {
				logger::error("No free FormID in ESP/ESM range for plugin: {}", targetFile->GetFilename());
				return false;
			}
			newFormID = (static_cast<std::uint32_t>(targetFile->compileIndex) << 24) | formCounts_[targetFile];
			if (!RE::TESDataHandler::GetSingleton()->LookupForm(newFormID, targetFile->GetFilename())) {
				formCounts_[targetFile]--;
				break;
			}
			logger::warn("FormID {:08X} already in use in plugin: {}. Trying next.",
				newFormID, targetFile->GetFilename());
			formCounts_[targetFile]--;
			attemptCount++;
		}
		if (attemptCount >= MAX_FORMID_ATTEMPTS) {
			logger::error("Failed to find free FormID for plugin: {} after {} attempts.",
				targetFile->GetFilename(), MAX_FORMID_ATTEMPTS);
			return false;
		}
	}

	form->formID = newFormID;
	form->SetFile(const_cast<RE::TESFile*>(targetFile));

	if (Settings::GetSingleton()->IsVerboseLogging()) {
		logger::info("Assigned new FormID {:08X} in plugin: {}",
			newFormID, targetFile->GetFilename());
	}

	return true;
}

const RE::TESFile* GetFileFromFormID(std::uint32_t formID)
{
	auto& dataHandler = *RE::TESDataHandler::GetSingleton();
	if ((formID & 0xFF000000) == 0xFE000000) {
		// ESL (light plugin)
		std::uint16_t smallIndex = static_cast<std::uint16_t>((formID & 0x00FFF000) >> 12);
		return dataHandler.LookupLoadedLightModByIndex(smallIndex);
	} else {
		// ESM/ESP
		std::uint8_t index = (formID & 0xFF000000) >> 24;
		return dataHandler.LookupLoadedModByIndex(index);
	}
}
