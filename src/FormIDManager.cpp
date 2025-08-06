#include "FormIDManager.h"
#include "Settings.h"

namespace Fax
{
	// ESL-Specific Constants
	constexpr std::uint32_t ESL_FORM_LIMIT = 2048;        // Max forms in ESL
	constexpr std::uint32_t ESL_HIGH_START = 0xFFF;       // Highest FormID for ESL
	constexpr std::uint32_t ESL_MIN = 0x800;              // Minimum FormID for ESL
	constexpr std::uint32_t ESL_FLAG = 0xFE000000;        // ESL FormID flag
	constexpr std::uint32_t ESL_INDEX_SHIFT = 12;         // ESL index shift
	constexpr std::uint32_t ESL_INDEX_MASK = 0x00FFF000;  // ESL index mask (bits 12-23)

	// ESP/ESM-Specific Constants
	constexpr std::uint32_t ESP_HIGH_START = 0xFFFFFF;    // Highest FormID for ESP/ESM
	constexpr std::uint32_t ESP_MIN = 0x800;              // Minimum FormID for ESP/ESM
	constexpr std::uint32_t ESP_INDEX_MASK = 0xFF000000;  // ESP/ESM index mask (bits 24-31)
	constexpr std::uint32_t ESP_INDEX_SHIFT = 24;         // ESP/ESM index shift

	// General Constants
	constexpr std::uint32_t MAX_FORMID_ATTEMPTS = 10;  // Max attempts to resolve conflicts
	constexpr const char* EMPTY_PLUGIN_NAME = "";      // Empty string for LookupForm calls
}

// Tracking of used ESL FormIDs
static std::bitset<Fax::ESL_FORM_LIMIT> usedESLFormIDs;

FormIDManager::FormIDManager() {}

bool FormIDManager::AssignFormID(RE::TESForm* form, const RE::TESFile* targetFile)
{
	if (!form || !targetFile) {
		logger::error("Invalid form or target file.");
		return false;
	}

	// Initialize counter for this plugin
	auto& counter = formCounts_[targetFile];
	auto& totalForms = formCountsTotal_[targetFile];
	const bool isLight = targetFile->IsLight();
	const bool verboseLogging = Settings::GetSingleton()->IsVerboseLogging();

	if (counter == 0) {
		// Reset counter to fixed constant for this plugin
		counter = isLight ? Fax::ESL_HIGH_START : Fax::ESP_HIGH_START;
		if (isLight) {
			usedESLFormIDs.reset();  // Reset bitset for ESL plugins
			if (verboseLogging) {
				logger::info("Reset counter for plugin {} to {:04X}",
					targetFile->GetFilename(), counter);
			}
		}
	}

	std::uint32_t newFormID = 0;
	std::uint32_t attemptCount = 0;

	// Early check for exhausted range
	if (isLight) {
		if (counter < Fax::ESL_MIN) {
			logger::error("Exhausted ESL FormID range for plugin: {}", targetFile->GetFilename());
			return false;
		}
	} else {
		if (counter < Fax::ESP_MIN) {
			logger::error("Exhausted ESP/ESM FormID range for plugin: {}", targetFile->GetFilename());
			return false;
		}
	}

	while (attemptCount < Fax::MAX_FORMID_ATTEMPTS) {
		// Construct FormID
		if (isLight) {
			newFormID = Fax::ESL_FLAG |
			            ((static_cast<std::uint32_t>(targetFile->smallFileCompileIndex) << Fax::ESL_INDEX_SHIFT) | counter);
		} else {
			newFormID = (static_cast<std::uint32_t>(targetFile->compileIndex) << Fax::ESP_INDEX_SHIFT) | counter;
		}

		// Check for invalid FormID 0
		if (newFormID == 0) {
			logger::error("Generated invalid FormID 0 for plugin: {}", targetFile->GetFilename());
			return false;
		}

		// Check FormID availability
		bool isAvailable = false;
		if (!RE::TESDataHandler::GetSingleton()->LookupForm(newFormID, Fax::EMPTY_PLUGIN_NAME)) {
			if (isLight) {
				if (counter >= Fax::ESL_MIN && counter <= Fax::ESL_HIGH_START) {
					std::uint32_t eslSlot = counter - Fax::ESL_MIN;
					if (!usedESLFormIDs.test(eslSlot)) {
						usedESLFormIDs.set(eslSlot);
						isAvailable = true;
					}
				}
			} else {
				isAvailable = true;
			}
		}

		if (isAvailable) {
			break;
		}

		// Log conflict for retries only
		if (verboseLogging && attemptCount > 0) {
			auto* conflictingForm = RE::TESDataHandler::GetSingleton()->LookupForm(newFormID, Fax::EMPTY_PLUGIN_NAME);
			const char* conflictEditorID = conflictingForm && conflictingForm->GetFormEditorID() ?
			                                   conflictingForm->GetFormEditorID() :
			                                   "Unknown";
			logger::warn("Retrying FormID {:08X} (Attempt {}/{}): In use by form (EditorID: {}, Type: {}) in plugin: {}.",
				newFormID, attemptCount + 1, Fax::MAX_FORMID_ATTEMPTS,
				conflictEditorID, conflictingForm ? conflictingForm->GetObjectTypeName() : "Unknown",
				targetFile->GetFilename());
		}

		counter--;
		attemptCount++;

		// Additional bounds check during loop
		if (isLight && counter < Fax::ESL_MIN) {
			logger::error("Exhausted ESL FormID range during assignment for plugin: {}", targetFile->GetFilename());
			return false;
		} else if (!isLight && counter < Fax::ESP_MIN) {
			logger::error("Exhausted ESP/ESM FormID range during assignment for plugin: {}", targetFile->GetFilename());
			return false;
		}
	}

	if (attemptCount >= Fax::MAX_FORMID_ATTEMPTS) {
		const char* editorID = form->GetFormEditorID() ? form->GetFormEditorID() : "Unknown";
		logger::error("Failed to find free FormID [{:08X}] for plugin: {} after {} attempts for EditorID: {}.",
			newFormID, targetFile->GetFilename(), Fax::MAX_FORMID_ATTEMPTS, editorID);
		return false;
	}

	// Assign FormID
	form->formID = newFormID;
	form->SetFile(const_cast<RE::TESFile*>(targetFile));
	counter--;
	totalForms++;

	return true;
}

const RE::TESFile* GetFileFromFormID(std::uint32_t formID)
{
	auto& dataHandler = *RE::TESDataHandler::GetSingleton();
	if ((formID & Fax::ESL_FLAG) == Fax::ESL_FLAG) {
		// ESL (light plugin)
		std::uint16_t smallIndex = static_cast<std::uint16_t>(
			(formID & Fax::ESL_INDEX_MASK) >> Fax::ESL_INDEX_SHIFT);
		return dataHandler.LookupLoadedLightModByIndex(smallIndex);
	} else {
		// ESM/ESP
		std::uint8_t index = static_cast<std::uint8_t>(
			(formID & Fax::ESP_INDEX_MASK) >> Fax::ESP_INDEX_SHIFT);
		return dataHandler.LookupLoadedModByIndex(index);
	}
}
