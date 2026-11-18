#pragma once
#include "../include/enshrouded_i18n.h"
#include <cassert>
#include <set>
#include <string>
template<size_t N> void checkCatalogue(const enshrouded::i18n::Translation(&rows)[N]) {
 std::set<std::string> english;
 std::set<std::pair<std::string,std::string>> distinct;
 for(const auto& row:rows) {
  assert(row.value && *row.value);
  assert(distinct.emplace(row.language,row.key).second);
  if(!std::strcmp(row.language,"en"))english.insert(row.key);
 }
 assert(!english.empty());
 assert(N==english.size()*15);
 for(const auto id:enshrouded::i18n::supportedLanguages) {
  const char* locale=enshrouded::i18n::code(id);
  for(const auto& key:english) {
   const char* exact=nullptr;
   for(const auto& row:rows)if(!std::strcmp(row.language,locale)&&row.key==key)exact=row.value;
   assert(exact); // A fallback translation must NOT count as language coverage.
   assert(enshrouded::i18n::find(rows,key.c_str(),id)==exact);
  }
 }
 for(const auto& key:english) {
  for(const auto id:{1u,2u,999u})
   assert(enshrouded::i18n::find(rows,key.c_str(),id)==enshrouded::i18n::find(rows,key.c_str(),0));
  assert(enshrouded::i18n::find(rows,key.c_str(),22)==enshrouded::i18n::find(rows,key.c_str(),5));
  assert(enshrouded::i18n::find(rows,key.c_str(),8)==enshrouded::i18n::find(rows,key.c_str(),7));
 }
 assert(!enshrouded::i18n::find(rows,"__missing_key__",0));
}
