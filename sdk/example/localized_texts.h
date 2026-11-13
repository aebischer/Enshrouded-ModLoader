#pragma once
#include "../include/enshrouded_i18n.h"

// Example catalog only: the ABI does not supply a native language service.
inline constexpr enshrouded::i18n::Translation ExampleTexts[] = {
    {"en", "item.name", "Example tool"},
    {"fr", "item.name", "Outil de démonstration"},
    {"en", "item.description", "An example localized item."},
    {"fr", "item.description", "Un objet de démonstration multilingue."},
};
inline const char* exampleText(const char* key, uint32_t nativeLanguageId) noexcept {
    return enshrouded::i18n::find(ExampleTexts, key, nativeLanguageId);
}
