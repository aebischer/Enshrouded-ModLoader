#pragma once
#include "../sdk/include/enshrouded_i18n.h"
#include "loader_texts.h"
static const char* localizedBoolean(bool value,uint32_t language) {
 return enshrouded::i18n::find(LoaderTexts,value?"bool.on":"bool.off",language);
}
