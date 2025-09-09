#include "FormIDManager.h"
#include "Settings.h"

namespace
{
	// General Constants
	constexpr std::uint32_t FORMID_MIN = 0x800;        // Minimum allowed FormID
	constexpr std::uint32_t MAX_FORMID_ATTEMPTS = 10;  // Maximum conflict resolution attempts

	// ESL (Light Plugin) Constants
	constexpr std::uint32_t ESL_FLAG = 0xFE000000;        // ESL FormID flag marker
	constexpr std::uint32_t ESL_HIGH_START = 0xFFF;       // Starting FormID for ESL (counts down)
	constexpr std::uint32_t ESL_INDEX_MASK = 0x00FFF000;  // Mask for ESL index (bits 12-23)
	constexpr std::uint32_t ESL_INDEX_SHIFT = 12;         // Bit shift for ESL index

	// ESP/ESM (Full Plugin) Constants
	constexpr std::uint32_t ESP_HIGH_START = 0xFFFFFF;    // Starting FormID for ESP/ESM (counts down)
	constexpr std::uint32_t ESP_INDEX_MASK = 0xFF000000;  // Mask for ESP/ESM index (bits 24-31)
	constexpr std::uint32_t ESP_INDEX_SHIFT = 24;         // Bit shift for ESP/ESM index
}

// Global tracking of used ESL FormIDs to prevent conflicts across all ESL plugins
static std::map<const RE::TESFile*, std::bitset<ESL_HIGH_START - FORMID_MIN + 1>> eslFormIDTracking;

FormIDManager::FormIDManager() {}

bool FormIDManager::AssignFormID(RE::TESForm* form, const RE::TESFile* targetFile)
{
	if (!form || !targetFile) {
		logger::error("Invalid form or target file provided for FormID assignment.");
		return false;
	}

	// Get or initialize counter for this plugin
	auto& counter = formCounts_[targetFile];
	const bool isLight = targetFile->IsLight();
	const bool verboseLogging = Settings::GetSingleton()->IsVerboseLogging();

	// Initialize counter on first use
	if (counter == 0) {
		counter = isLight ? ESL_HIGH_START : ESP_HIGH_START;
		if (verboseLogging) {
			if (isLight) {
				logger::info("Initialized FormID counter for ESL plugin {} to {:04X}",
					targetFile->GetFilename(), counter);
			} else {
				logger::info("Initialized FormID counter for ESP/ESM plugin {} to {:06X}",
					targetFile->GetFilename(), counter);
			}
		}
	}

	std::uint32_t newFormID = 0;
	std::uint32_t attemptCount = 0;
	std::uint32_t currentCounter = counter;

	// Attempt to find an available FormID
	while (attemptCount < MAX_FORMID_ATTEMPTS) {
		// Early validation - check if we've exhausted the FormID range
		if (currentCounter < FORMID_MIN) {
			if (isLight) {
				logger::error("Exhausted ESL FormID range for plugin: {}", targetFile->GetFilename());
			} else {
				logger::error("Exhausted ESP/ESM FormID range for plugin: {}", targetFile->GetFilename());
			}
			return false;
		}

		// Construct FormID based on plugin type
		if (isLight) {
			newFormID = ESL_FLAG |
			            ((static_cast<std::uint32_t>(targetFile->smallFileCompileIndex) << ESL_INDEX_SHIFT) | currentCounter);
		} else {
			newFormID = (static_cast<std::uint32_t>(targetFile->compileIndex) << ESP_INDEX_SHIFT) | currentCounter;
		}

		// Validate FormID is not zero (invalid)
		if (newFormID == 0) {
			logger::error("Generated invalid FormID 0 for plugin: {}", targetFile->GetFilename());
			return false;
		}

		// Check if FormID is available
		auto* existingForm = RE::TESDataHandler::GetSingleton()->LookupForm(newFormID, "");
		bool isAvailable = !existingForm;

		if (isAvailable && isLight) {
			// Additional ESL conflict tracking per plugin
			auto& eslTracking = eslFormIDTracking[targetFile];
			if (currentCounter >= FORMID_MIN && currentCounter <= ESL_HIGH_START) {
				const std::uint32_t eslSlot = currentCounter - FORMID_MIN;
				if (eslSlot < eslTracking.size() && !eslTracking.test(eslSlot)) {
					eslTracking.set(eslSlot);
				} else {
					isAvailable = false;  // Already used in our tracking
				}
			} else {
				isAvailable = false;  // Out of valid ESL range
			}
		}

		if (isAvailable) {
			// Successfully found available FormID - assign it using SetFormID and update counter
			form->SetFormID(newFormID, false);
			counter = currentCounter - 1;  // Update the persistent counter

			if (verboseLogging) {
				const char* editorID = form->GetFormEditorID() ? form->GetFormEditorID() : "Unknown";
				logger::info("Assigned FormID {:08X} to '{}' in plugin '{}'",
					newFormID, editorID, targetFile->GetFilename());
			}

			return true;
		}

		// Log conflict details for debugging
		if (verboseLogging) {
			if (existingForm) {
				const char* conflictEditorID = existingForm->GetFormEditorID() ?
				                                   existingForm->GetFormEditorID() :
				                                   "Unknown";
				logger::warn("FormID conflict {:08X} (Attempt {}/{}): Used by form '{}' (Type: {}) in plugin: {}",
					newFormID, attemptCount + 1, MAX_FORMID_ATTEMPTS,
					conflictEditorID, existingForm->GetObjectTypeName(),
					targetFile->GetFilename());
			} else if (isLight) {
				logger::warn("ESL FormID {:08X} conflicts with internal tracking (Attempt {}/{})",
					newFormID, attemptCount + 1, MAX_FORMID_ATTEMPTS);
			}
		}

		// Move to next FormID
		currentCounter--;
		attemptCount++;
	}

	// Failed to find an available FormID after all attempts
	const char* editorID = form->GetFormEditorID() ? form->GetFormEditorID() : "Unknown";
	logger::error("Failed to find available FormID for '{}' in plugin '{}' after {} attempts",
		editorID, targetFile->GetFilename(), MAX_FORMID_ATTEMPTS);
	return false;
}

const RE::TESFile* GetFileFromFormID(std::uint32_t formID)
{
	auto& dataHandler = *RE::TESDataHandler::GetSingleton();

	if ((formID & ESL_FLAG) == ESL_FLAG) {
		// Handle ESL (light plugin) FormID
		const std::uint16_t smallIndex = static_cast<std::uint16_t>(
			(formID & ESL_INDEX_MASK) >> ESL_INDEX_SHIFT);
		return dataHandler.LookupLoadedLightModByIndex(smallIndex);
	} else {
		// Handle ESP/ESM (full plugin) FormID
		const std::uint8_t index = static_cast<std::uint8_t>(
			(formID & ESP_INDEX_MASK) >> ESP_INDEX_SHIFT);
		return dataHandler.LookupLoadedModByIndex(index);
	}
}
