#include "../src/loader.cpp"
#include <fstream>
#include <cassert>
int main() {
 const auto root=std::filesystem::current_path()/("i18n-test-"+std::to_string(GetCurrentProcessId()));
 std::filesystem::create_directories(root/L"ModLoader");std::filesystem::create_directories(root/L"Mods");
 folder=(root/L"ModLoader").wstring();std::ofstream(root/L"Mods"/L"sample.dll")<<"not executable";
 const std::string manifest="ENSHROUDED_PACKAGE_V1\nkind=mod\nversion=1.0\nabi=1\nsetting=mode|choice|Mode|low|low|high\n";
 auto parse=[&](const std::string& suffix,Package& p) {
  std::ofstream(root/L"Mods"/L"sample.dll.mod",std::ios::binary)<<manifest<<suffix;
  std::string error;return readPackage("sample.dll",p,error);
 };
 Package p;const std::string fallback="fallback";
 assert(parse("text=en|setting.mode|Mode\ntext=fr|setting.mode|Mode français\ntext=es|setting.mode|Modo\ntext=fr|choice.mode.0|Faible\n",p));
 assert(packageText(p,"setting.mode",fallback,5)=="Mode français");
 assert(packageText(p,"setting.mode",fallback,22)=="Mode français");
 assert(packageText(p,"setting.mode",fallback,8)=="Modo");
 assert(packageText(p,"setting.mode",fallback,999)=="Mode");
 assert(packageText(p,"choice.mode.0",fallback,5)=="Faible");
 assert(p.settings[0].defaultValue=="low"&&p.settings[0].choices[0]=="low");
 assert(packageText(p,"unknown",fallback,5)==fallback);
 assert(!parse("text=fr|setting.mode|A\ntext=fr|setting.mode|B\n",p));
 assert(!parse("text=xx|setting.mode|A\n",p));
 assert(!parse("text=fr|setting.missing|A\n",p));
 assert(!parse("text=fr|choice.mode.2|A\n",p));
 assert(!parse("text=fr|setting.mode|\n",p));
 assert(!parse("text=fr|setting.mode|"+std::string(101,'a')+"\n",p));
 assert(!parse(std::string("text=fr|setting.mode|")+char(0xff)+"\n",p));
 assert(parse("",p));assert(packageText(p,"setting.mode",p.settings[0].label,5)=="Mode");
 assert(!strcmp(localizedBoolean(true,5),"Activé"));assert(!strcmp(localizedBoolean(false,999),"Disabled"));
 assert(gameLanguage()==1); // outside the game; must not dereference native offsets
 puts("PASS: UTF-8 localization, region fallback, unknown language, labels/values separation, legacy manifests, invalid and duplicate translations.");
}
