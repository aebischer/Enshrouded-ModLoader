#pragma once
#include "../sdk/include/enshrouded_i18n.h"
#include "loader_texts.h"
static const char* loaderText(const char* key) {
 const auto result=enshrouded::i18n::find(LoaderTexts,key,gameLanguage());
 return result?result:key;
}
static bool dialogText(const char* key,uint32_t language,wchar_t* output,int capacity) noexcept {
 const auto value=enshrouded::i18n::find(LoaderTexts,key,language);
 return value && MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value,-1,output,capacity)>0;
}
static void loaderErrorDialog(const char* bodyKey,const char* titleKey,UINT flags) noexcept {
 wchar_t body[2048]{},title[256]{};
 const auto language=dialogLanguage();
 if(!dialogText(bodyKey,language,body,2048))
  wcscpy_s(body,L"ModLoader failed. Close the game and check ModLoader/mod-loader.log.");
 if(!dialogText(titleKey,language,title,256)) wcscpy_s(title,L"Enshrouded - ModLoader error");
 MessageBoxW(nullptr,body,title,flags);
}
