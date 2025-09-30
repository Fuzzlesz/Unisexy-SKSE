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

	// Generate deterministic FormID using only EditorID
	std::uint32_t GenerateBaseFormID(const std::string& editorID, bool isLight)
	{
		// Use EditorID for determinism
		std::hash<std::string> hasher;
		size_t hash = hasher(editorID);
		std::uint32_t maxFormID = isLight ? ESL_HIGH_START : ESP_HIGH_START;
		std::uint32_t range = maxFormID - FORMID_MIN + 1;
		return maxFormID - (static_cast<std::uint32_t>(hash % range));
	}
}

FormIDManager::FormIDManager() {}

bool FormIDManager::AssignFormID(RE::TESForm* form, const RE::TESFile* targetFile, std::uint32_t& outConflictFormID)
{
	// Validate inputs
	if (!form || !targetFile) {
		logger::error("Invalid form or target file provided for FormID assignment.");
		return false;
	}

	const char* editorID = form->GetFormEditorID();
	if (!editorID || editorID[0] == '\0') {
		logger::error("No EditorID for form in plugin: {}", targetFile->GetFilename());
		return false;
	}

	// Get plugin properties and tracking
	const bool isLight = targetFile->IsLight();
	const bool verboseLogging = Settings::GetSingleton()->IsVerboseLogging();
	auto& assignedIDs = assignedFormIDs_[targetFile];

	// Generate initial FormID
	std::uint32_t counter = GenerateBaseFormID(editorID, isLight);
	if (verboseLogging) {
		logger::info("Generated FormID counter {:04X} for '{}'", counter, editorID);
	}

	std::uint32_t newFormID = 0;
	std::uint32_t attemptCount = 0;
	outConflictFormID = 0;  // Initialize output parameter

	// Attempt to find an available FormID
	while (attemptCount < MAX_FORMID_ATTEMPTS) {
		// Check FormID range
		if (counter < FORMID_MIN) {
			if (isLight) {
				logger::error("Exhausted ESL FormID range for plugin: {}", std::string(targetFile->GetFilename()));
			} else {
				logger::error("Exhausted ESP/ESM FormID range for plugin: {}", std::string(targetFile->GetFilename()));
			}
			return false;
		}

		// Construct FormID based on plugin type
		if (isLight) {
			newFormID = ESL_FLAG |
			            ((static_cast<std::uint32_t>(targetFile->smallFileCompileIndex) << ESL_INDEX_SHIFT) | counter);
		} else {
			newFormID = ((static_cast<std::uint32_t>(targetFile->compileIndex) << ESP_INDEX_SHIFT) | counter);
		}

		// Validate FormID
		if (newFormID == 0) {
			logger::error("Generated invalid FormID 0 for plugin: {}", std::string(targetFile->GetFilename()));
			return false;
		}

		// Check FormID availability within the target plugin
		bool isAvailable = assignedIDs.find(newFormID) == assignedIDs.end();

		if (isAvailable) {
			// Double-check with data handler for forms in the target plugin
			auto* existingForm = RE::TESDataHandler::GetSingleton()->LookupForm(newFormID, targetFile->GetFilename());
			if (!existingForm) {
				form->SetFormID(newFormID, false);
				assignedIDs.insert(newFormID);
				if (verboseLogging) {
					logger::info("Assigned FormID {:08X} to '{}' in plugin '{}'",
						newFormID, editorID, std::string(targetFile->GetFilename()));
					if (outConflictFormID != 0) {
						logger::info("Resolved conflict for FormID {:08X} by assigning {:08X}", outConflictFormID, newFormID);
					}
				}
				return true;
			}
			// Conflict found in the target plugin
			isAvailable = false;
			outConflictFormID = newFormID;
			if (verboseLogging) {
				const char* conflictEditorID = existingForm->GetFormEditorID() ? existingForm->GetFormEditorID() : "Unknown";
				logger::warn("FormID conflict {:08X} (Attempt {}/{}): Used by form '{}' (Type: {}) in plugin: {}",
					newFormID, attemptCount + 1, MAX_FORMID_ATTEMPTS,
					conflictEditorID, existingForm->GetObjectTypeName(),
					std::string(targetFile->GetFilename()));
			}
		} else {
			// Conflict within assignedIDs
			outConflictFormID = newFormID;
			if (verboseLogging) {
				logger::warn("FormID {:08X} already assigned in plugin: {} (Attempt {}/{})",
					newFormID, std::string(targetFile->GetFilename()), attemptCount + 1, MAX_FORMID_ATTEMPTS);
			}
		}

		// Deterministic fallback: decrement counter
		counter--;
		attemptCount++;
	}

	logger::error("Failed to assign FormID for '{}' in plugin '{}' after {} attempts",
		editorID, std::string(targetFile->GetFilename()), MAX_FORMID_ATTEMPTS);
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
