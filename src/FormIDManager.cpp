#include "FormIDManager.h"
#include "Settings.h"

namespace Fax
{
	// ESL (Light Plugin) Constants
	constexpr std::uint32_t ESL_FORM_LIMIT = 2048;        // Maximum forms allowed in ESL
	constexpr std::uint32_t ESL_HIGH_START = 0xFFF;       // Starting FormID for ESL (counts down)
	constexpr std::uint32_t ESL_MIN = 0x800;              // Minimum allowed FormID for ESL
	constexpr std::uint32_t ESL_FLAG = 0xFE000000;        // ESL FormID flag marker
	constexpr std::uint32_t ESL_INDEX_SHIFT = 12;         // Bit shift for ESL index
	constexpr std::uint32_t ESL_INDEX_MASK = 0x00FFF000;  // Mask for ESL index (bits 12-23)

	// ESP/ESM (Full Plugin) Constants
	constexpr std::uint32_t ESP_HIGH_START = 0xFFFFFF;    // Starting FormID for ESP/ESM (counts down)
	constexpr std::uint32_t ESP_MIN = 0x800;              // Minimum allowed FormID for ESP/ESM
	constexpr std::uint32_t ESP_INDEX_MASK = 0xFF000000;  // Mask for ESP/ESM index (bits 24-31)
	constexpr std::uint32_t ESP_INDEX_SHIFT = 24;         // Bit shift for ESP/ESM index

	// General Constants
	constexpr std::uint32_t MAX_FORMID_ATTEMPTS = 10;  // Maximum conflict resolution attempts
	constexpr const char* EMPTY_PLUGIN_NAME = "";      // Empty string for LookupForm calls
}

// Global tracking of used ESL FormIDs to prevent conflicts
static std::bitset<Fax::ESL_FORM_LIMIT> usedESLFormIDs;

FormIDManager::FormIDManager() {}

bool FormIDManager::AssignFormID(RE::TESForm* form, const RE::TESFile* targetFile)
{
	if (!form || !targetFile) {
		logger::error("Invalid form or target file provided for FormID assignment.");
		return false;
	}

	// Get or initialize counter for this plugin
	auto& counter = formCounts_[targetFile];
	auto& totalForms = formCountsTotal_[targetFile];
	const bool isLight = targetFile->IsLight();
	const bool verboseLogging = Settings::GetSingleton()->IsVerboseLogging();

	// Initialize counter on first use
	if (counter == 0) {
		counter = isLight ? Fax::ESL_HIGH_START : Fax::ESP_HIGH_START;
		if (isLight) {
			usedESLFormIDs.reset();  // Clear ESL tracking for new plugin
			if (verboseLogging) {
				logger::info("Initialized FormID counter for ESL plugin {} to {:04X}",
					targetFile->GetFilename(), counter);
			}
		} else if (verboseLogging) {
			logger::info("Initialized FormID counter for ESP/ESM plugin {} to {:06X}",
				targetFile->GetFilename(), counter);
		}
	}

	std::uint32_t newFormID = 0;
	std::uint32_t attemptCount = 0;

	// Early validation - check if we've exhausted the FormID range
	if (isLight && counter < Fax::ESL_MIN) {
		logger::error("Exhausted ESL FormID range for plugin: {}", targetFile->GetFilename());
		return false;
	} else if (!isLight && counter < Fax::ESP_MIN) {
		logger::error("Exhausted ESP/ESM FormID range for plugin: {}", targetFile->GetFilename());
		return false;
	}

	// Attempt to find an available FormID
	while (attemptCount < Fax::MAX_FORMID_ATTEMPTS) {
		// Construct FormID based on plugin type
		if (isLight) {
			newFormID = Fax::ESL_FLAG |
			            ((static_cast<std::uint32_t>(targetFile->smallFileCompileIndex) << Fax::ESL_INDEX_SHIFT) | counter);
		} else {
			newFormID = (static_cast<std::uint32_t>(targetFile->compileIndex) << Fax::ESP_INDEX_SHIFT) | counter;
		}

		// Validate FormID is not zero (invalid)
		if (newFormID == 0) {
			logger::error("Generated invalid FormID 0 for plugin: {}", targetFile->GetFilename());
			return false;
		}

		// Check if FormID is available
		bool isAvailable = false;
		if (!RE::TESDataHandler::GetSingleton()->LookupForm(newFormID, Fax::EMPTY_PLUGIN_NAME)) {
			if (isLight) {
				// Additional ESL conflict tracking
				if (counter >= Fax::ESL_MIN && counter <= Fax::ESL_HIGH_START) {
					const std::uint32_t eslSlot = counter - Fax::ESL_MIN;
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

		// Log conflict details for debugging (only on retries to reduce spam)
		if (verboseLogging && attemptCount > 0) {
			auto* conflictingForm = RE::TESDataHandler::GetSingleton()->LookupForm(newFormID, Fax::EMPTY_PLUGIN_NAME);
			const char* conflictEditorID = conflictingForm && conflictingForm->GetFormEditorID() ?
			                                   conflictingForm->GetFormEditorID() :
			                                   "Unknown";
			logger::warn("FormID conflict {:08X} (Attempt {}/{}): Used by form '{}' (Type: {}) in plugin: {}",
				newFormID, attemptCount + 1, Fax::MAX_FORMID_ATTEMPTS,
				conflictEditorID, conflictingForm ? conflictingForm->GetObjectTypeName() : "Unknown",
				targetFile->GetFilename());
		}

		// Move to next FormID and check bounds
		counter--;
		attemptCount++;

		if (isLight && counter < Fax::ESL_MIN) {
			logger::error("Exhausted ESL FormID range during assignment for plugin: {}", targetFile->GetFilename());
			return false;
		} else if (!isLight && counter < Fax::ESP_MIN) {
			logger::error("Exhausted ESP/ESM FormID range during assignment for plugin: {}", targetFile->GetFilename());
			return false;
		}
	}

	// Check if we failed to find an available FormID
	if (attemptCount >= Fax::MAX_FORMID_ATTEMPTS) {
		const char* editorID = form->GetFormEditorID() ? form->GetFormEditorID() : "Unknown";
		logger::error("Failed to find available FormID for '{}' in plugin '{}' after {} attempts",
			editorID, targetFile->GetFilename(), Fax::MAX_FORMID_ATTEMPTS);
		return false;
	}

	// Successfully found available FormID - assign it
	form->formID = newFormID;
	form->SetFile(const_cast<RE::TESFile*>(targetFile));
	counter--;  // Prepare for next allocation
	totalForms++;

	if (verboseLogging) {
		const char* editorID = form->GetFormEditorID() ? form->GetFormEditorID() : "Unknown";
		logger::debug("Assigned FormID {:08X} to '{}' in plugin '{}'",
			newFormID, editorID, targetFile->GetFilename());
	}

	return true;
}

const RE::TESFile* GetFileFromFormID(std::uint32_t formID)
{
	auto& dataHandler = *RE::TESDataHandler::GetSingleton();

	if ((formID & Fax::ESL_FLAG) == Fax::ESL_FLAG) {
		// Handle ESL (light plugin) FormID
		const std::uint16_t smallIndex = static_cast<std::uint16_t>(
			(formID & Fax::ESL_INDEX_MASK) >> Fax::ESL_INDEX_SHIFT);
		return dataHandler.LookupLoadedLightModByIndex(smallIndex);
	} else {
		// Handle ESP/ESM (full plugin) FormID
		const std::uint8_t index = static_cast<std::uint8_t>(
			(formID & Fax::ESP_INDEX_MASK) >> Fax::ESP_INDEX_SHIFT);
		return dataHandler.LookupLoadedModByIndex(index);
	}
}
