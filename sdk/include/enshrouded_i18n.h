#pragma once
#include <cstdint>
#include <cstring>
// Native language IDs of the supported client. No Windows/Steam locale guessing.
namespace enshrouded::i18n {
// The game's 15 supported interface languages, in native ID space (not 0..14).
inline constexpr uint32_t supportedLanguages[]={0,5,6,3,7,11,13,10,16,9,20,21,18,34,17};
inline const char* code(uint32_t language) noexcept {
 static constexpr const char* codes[]={"en","en-us","en-gb","de","nl","fr","it","es","es-mx","ru","pl","ja","id","ko","ms","pt","pt-br","th","tr","vi","zh-cn","zh-tw","fr-ca","es-419","bg","cs","da","fi","el","hu","no","nb","ro","sv","uk"};
 return language<sizeof(codes)/sizeof(*codes)?codes[language]:"en";
}
inline const char* parent(const char* language) noexcept {
 if(!std::strcmp(language,"fr-ca"))return "fr";
 if(!std::strcmp(language,"es-mx"))return "es-419";
 if(!std::strcmp(language,"es-419"))return "es";
 if(!std::strcmp(language,"pt-br"))return "pt";
 if(!std::strcmp(language,"nb"))return "no";
 return "en";
}
inline bool validCode(const char* language) noexcept {
 for(uint32_t i=0;i<35;++i)if(!std::strcmp(language,code(i)))return true;
 return false;
}
struct Translation {const char* language;const char* key;const char* value;};
template<size_t N> const char* find(const Translation(&catalog)[N],const char* key,uint32_t language) noexcept {
 const char* wanted=code(language);
 for(unsigned depth=0;depth<4;++depth) {
  for(const auto& row:catalog)if(!std::strcmp(row.language,wanted)&&!std::strcmp(row.key,key))return row.value;
  if(!std::strcmp(wanted,"en"))break;
  wanted=parent(wanted);
 }
 return nullptr;
}
}
