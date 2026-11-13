#pragma once
#include "../sdk/include/enshrouded_i18n.h"
static const char* localizedBoolean(bool value,uint32_t language) {
 static constexpr enshrouded::i18n::Translation texts[]={
 {"en","off","Disabled"},{"en","on","Enabled"},
 {"fr","off","Désactivé"},{"fr","on","Activé"},
 {"de","off","Deaktiviert"},{"de","on","Aktiviert"},
 {"it","off","Disattivato"},{"it","on","Attivato"},
 {"es","off","Desactivado"},{"es","on","Activado"},
 {"ru","off","Выкл."},{"ru","on","Вкл."},
 {"pl","off","Wyłączone"},{"pl","on","Włączone"},
 {"ja","off","無効"},{"ja","on","有効"},
 {"ko","off","비활성화"},{"ko","on","활성화"},
 {"pt-br","off","Desativado"},{"pt-br","on","Ativado"},
 {"th","off","ปิด"},{"th","on","เปิด"},
 {"tr","off","Devre dışı"},{"tr","on","Etkin"},
 {"zh-cn","off","已禁用"},{"zh-cn","on","已启用"},
 {"zh-tw","off","已停用"},{"zh-tw","on","已啟用"},
 {"uk","off","Вимкнено"},{"uk","on","Увімкнено"}
 };
 return enshrouded::i18n::find(texts,value?"on":"off",language);
}
