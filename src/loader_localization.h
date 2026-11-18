#pragma once
#include "../sdk/include/enshrouded_i18n.h"
#include "loader_texts.h"
static const char* loaderText(const char* key) {
 const auto result=enshrouded::i18n::find(LoaderTexts,key,gameLanguage());
 return result?result:key;
}
