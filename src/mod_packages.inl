// Declarative contract + non-executing PE validation. Native plugins remain trusted code.
#include "../sdk/include/enshrouded_i18n.h"
struct SettingSpec {
    std::string key,label,defaultValue;
    bool boolean=false,percent=false;
    int minimum=0,maximum=0,step=1;
    std::vector<std::string> choices;
};
static bool settingInteger(const std::string& value,int& result) {
    if(value.empty() || value.size()>5 || (value.size()>1 && value[0]=='0')) return false;
    result=0;
    for(char c:value) {if(c<'0' || c>'9') return false; result=result*10+c-'0';}
    return result<=10000;
}
static bool settingKey(const std::string& key) {
    return !key.empty() && key.size()<=40 && key.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_")==std::string::npos;
}
static std::vector<std::string> splitSetting(const std::string& value) {
    std::vector<std::string> parts; size_t begin=0;
    for(;;) {const auto end=value.find('|',begin); parts.push_back(value.substr(begin,end-begin)); if(end==std::string::npos) break; begin=end+1;}
    return parts;
}
struct Package {
    std::string id,version,manifest;
    bool mod=false;
    std::vector<SettingSpec> settings;
    std::filesystem::path path;
    std::map<std::string,std::string> dependencies;
    std::map<std::string,std::map<std::string,std::string>> translations;
};
static const std::string& packageText(const Package& item,const std::string& key,const std::string& fallback,uint32_t language) {
    const char* wanted=enshrouded::i18n::code(language);
    for(unsigned depth=0;depth<4;++depth) {
        const auto locale=item.translations.find(wanted);
        if(locale!=item.translations.end()) {
            const auto entry=locale->second.find(key);
            if(entry!=locale->second.end()) return entry->second;
        }
        if(!strcmp(wanted,"en")) break;
        wanted=enshrouded::i18n::parent(wanted);
    }
    return fallback;
}
static bool packageVersion(const std::string& s) {return !s.empty() && s.size()<=64 && s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-")==std::string::npos;}
static bool readPackage(const std::string& id,Package& item,std::string& error) {
    item=Package{}; item.id=id;
    auto fail=[&] {error="Manifeste invalide ou ABI incompatible : "+id; return false;};
    if(!modPath(id,item.path)) {error="Fichier absent ou chemin interdit : "+id; return false;}
    std::string body;
    if(!boundedRead(item.path.wstring()+L".mod",16384,body)) {error="Manifeste .dll.mod absent ou illisible : "+id; return false;}
    item.manifest=body;
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,body.data(),static_cast<int>(body.size()),nullptr,0)) return fail();
    if(body.compare(0,3,"\xef\xbb\xbf")==0) body.erase(0,3);
    std::istringstream in(body); std::string line; bool header=false,kind=false,version=false,abi=false;
    while(std::getline(in,line)) {
        if(!line.empty() && line.back()=='\r') line.pop_back();
        if(!header) {if(line!="ENSHROUDED_PACKAGE_V1") return fail(); header=true; continue;}
        if(line.empty()) continue;
        const auto split=line.find('='); if(split==std::string::npos) return fail();
        const auto k=line.substr(0,split),v=line.substr(split+1);
        if(k=="kind" && !kind && (v=="mod" || v=="library")) {kind=true; item.mod=v=="mod";}
        else if(k=="version" && !version && packageVersion(v)) {version=true; item.version=v;}
        else if(k=="abi" && !abi && v=="1") abi=true;
        else if(k=="setting" && item.settings.size()<16) {
            auto parts=splitSetting(v);
            if(parts.size()<4 || !settingKey(parts[0]) || parts[2].empty() || parts[2].size()>100) return fail();
            for(const auto& part:parts) if(part.size()>100 || part.find_first_of("\r\n\t=")!=std::string::npos || part.find('\0')!=std::string::npos) return fail();
            if(std::any_of(item.settings.begin(),item.settings.end(),[&](const SettingSpec& setting){return setting.key==parts[0];})) return fail();
            SettingSpec spec; spec.key=parts[0]; spec.label=parts[2]; spec.defaultValue=parts[3];
            if(parts[1]=="bool" && parts.size()==4 && (parts[3]=="true" || parts[3]=="false")) spec.boolean=true;
            else if(parts[1]=="choice" && parts.size()>=6 && parts.size()<=12) {
                std::set<std::string> distinct;
                for(size_t i=4;i<parts.size();++i) if(parts[i].empty() || !distinct.insert(parts[i]).second) return fail();
                spec.choices.assign(parts.begin()+4,parts.end());
                if(!distinct.count(spec.defaultValue)) return fail();
            } else if(parts[1]=="percent" && parts.size()==7) {
                int initial;
                if(!settingInteger(parts[3],initial) || !settingInteger(parts[4],spec.minimum) ||
                   !settingInteger(parts[5],spec.maximum) || !settingInteger(parts[6],spec.step) ||
                   spec.step<1 || spec.maximum<=spec.minimum || (spec.maximum-spec.minimum)%spec.step ||
                   (spec.maximum-spec.minimum)/spec.step>31 || initial<spec.minimum || initial>spec.maximum ||
                   (initial-spec.minimum)%spec.step) return fail();
                spec.percent=true;
            } else return fail();
            item.settings.push_back(std::move(spec));
        }
        else if(k=="text") {
            const auto parts=splitSetting(v);
            if(parts.size()!=3 || !enshrouded::i18n::validCode(parts[0].c_str()) ||
               parts[1].empty() || parts[1].size()>100 || parts[1].find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_.")!=std::string::npos ||
               parts[2].empty() || parts[2].size()>100 || parts[2].find_first_of("\r\n\t=")!=std::string::npos || parts[2].find('\0')!=std::string::npos ||
               !item.translations[parts[0]].emplace(parts[1],parts[2]).second) return fail();
        }
        else if(k=="require" && item.dependencies.size()<64) {
            const auto at=v.find('@'); if(at==std::string::npos) return fail();
            const auto dep=modId(v.substr(0,at)),ver=v.substr(at+1);
            if(dep.empty() || !packageVersion(ver) || !item.dependencies.emplace(dep,ver).second) return fail();
        } else return fail();
    }
    if(!in.eof() || !header || !kind || !version || !abi || (!item.mod && !item.settings.empty())) {error="Manifeste invalide ou ABI incompatible : "+id; return false;}
    for(const auto& locale:item.translations) for(const auto& entry:locale.second) {
        bool known=false;
        for(const auto& spec:item.settings) {
            if(entry.first=="setting."+spec.key) known=true;
            for(size_t i=0;i<spec.choices.size();++i)
                if(entry.first=="choice."+spec.key+"."+std::to_string(i)) known=true;
        }
        if(!known) return fail();
    }
    return true;
}
static std::string lowerAscii(std::string s) {for(auto& c:s) if(c>='A' && c<='Z') c+=('a'-'A'); return s;}
struct PeFacts {std::set<std::string> imports; bool initialize=false,configure=false;};
// No pointers into the input survive this parser. Every RVA and multiplication is bounded.
static bool inspectPe(const std::string& bytes,PeFacts& facts) {
    facts={};
    auto read=[&](size_t offset,void* dest,size_t n) {if(offset>bytes.size() || n>bytes.size()-offset) return false; memcpy(dest,bytes.data()+offset,n); return true;};
    IMAGE_DOS_HEADER dos{}; IMAGE_NT_HEADERS64 nt{};
    if(!read(0,&dos,sizeof dos) || dos.e_magic!=IMAGE_DOS_SIGNATURE || dos.e_lfanew<0 ||
       !read(static_cast<size_t>(dos.e_lfanew),&nt,sizeof nt) || nt.Signature!=IMAGE_NT_SIGNATURE ||
       nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64 || !(nt.FileHeader.Characteristics&IMAGE_FILE_DLL) ||
       nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt.FileHeader.SizeOfOptionalHeader!=sizeof(IMAGE_OPTIONAL_HEADER64) ||
       nt.FileHeader.NumberOfSections==0 || nt.FileHeader.NumberOfSections>96 || nt.OptionalHeader.NumberOfRvaAndSizes<16) return false;
    std::vector<IMAGE_SECTION_HEADER> sections(nt.FileHeader.NumberOfSections);
    if(!read(static_cast<size_t>(dos.e_lfanew)+sizeof nt,sections.data(),sections.size()*sizeof(IMAGE_SECTION_HEADER))) return false;
    auto rva=[&](uint32_t address,void* dest,size_t n) {
        if(address<nt.OptionalHeader.SizeOfHeaders && n<=nt.OptionalHeader.SizeOfHeaders-address) return read(address,dest,n);
        for(const auto& s:sections) if(address>=s.VirtualAddress) {
            const uint64_t delta=uint64_t(address)-s.VirtualAddress;
            if(delta<=s.SizeOfRawData && n<=s.SizeOfRawData-delta) return read(static_cast<size_t>(uint64_t(s.PointerToRawData)+delta),dest,n);
        }
        return false;
    };
    auto stringAt=[&](uint32_t address,std::string& out) {
        out.clear(); for(uint32_t i=0;i<512;++i) {char c;
            if(address>UINT32_MAX-i || !rva(address+i,&c,1)) return false;
            if(!c) return !out.empty();
            if(static_cast<unsigned char>(c)<32 || static_cast<unsigned char>(c)>126) return false;
            out+=c;
        } return false;
    };
    auto importName=[&](uint32_t address) {
        std::string name;
        if(!stringAt(address,name) || name.find_first_of("/\\:")!=std::string::npos) return false;
        name=lowerAscii(name); if(name.size()<5 || name.substr(name.size()-4)!=".dll") return false;
        facts.imports.insert(name); return facts.imports.size()<=256;
    };
    const auto imports=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if(imports.VirtualAddress) {
        bool ended=false;
        for(uint32_t i=0;i<257;++i) {
            IMAGE_IMPORT_DESCRIPTOR desc{};
            const uint64_t address=uint64_t(imports.VirtualAddress)+i*sizeof desc;
            if((i+1)*sizeof desc>imports.Size || address>UINT32_MAX || !rva(static_cast<uint32_t>(address),&desc,sizeof desc)) return false;
            if(!desc.Name) {ended=true; break;}
            if(!importName(desc.Name)) return false;
        }
        if(!ended) return false;
    }
    const auto delay=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT];
    if(delay.VirtualAddress) {
        bool ended=false;
        for(uint32_t i=0;i<257;++i) {
            uint32_t d[8]{}; const uint64_t address=uint64_t(delay.VirtualAddress)+i*sizeof d;
            if((i+1)*sizeof d>delay.Size || address>UINT32_MAX || !rva(static_cast<uint32_t>(address),d,sizeof d)) return false;
            if(!d[1]) {ended=true; break;}
            if(d[0]!=1 || !importName(d[1])) return false;
        } if(!ended) return false;
    }
    const auto exports=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if(exports.VirtualAddress) {
        IMAGE_EXPORT_DIRECTORY exp{};
        if(!rva(exports.VirtualAddress,&exp,sizeof exp) || exp.NumberOfNames>65536 || exp.NumberOfFunctions>65536) return false;
        for(uint32_t i=0;i<exp.NumberOfNames;++i) {
            uint32_t name{}; const uint64_t address=uint64_t(exp.AddressOfNames)+uint64_t(i)*4;
            if(address>UINT32_MAX || !rva(static_cast<uint32_t>(address),&name,4)) return false;
            std::string s; if(!stringAt(name,s)) return false;
            if(s=="EnshroudedModInitialize" || s=="EnshroudedModConfigure") {
                uint16_t ordinal; uint32_t function;
                const uint64_t oa=uint64_t(exp.AddressOfNameOrdinals)+uint64_t(i)*2;
                if(oa>UINT32_MAX || !rva(static_cast<uint32_t>(oa),&ordinal,2) || ordinal>=exp.NumberOfFunctions) return false;
                const uint64_t fa=uint64_t(exp.AddressOfFunctions)+uint64_t(ordinal)*4;
                if(fa>UINT32_MAX || !rva(static_cast<uint32_t>(fa),&function,4) || !function) return false;
                if(function>=exports.VirtualAddress && uint64_t(function)<uint64_t(exports.VirtualAddress)+exports.Size) return false; // no forwarded init
                bool executable=false;
                for(const auto& sec:sections) if(function>=sec.VirtualAddress && uint64_t(function)-sec.VirtualAddress<sec.Misc.VirtualSize && (sec.Characteristics&IMAGE_SCN_MEM_EXECUTE)) executable=true;
                if(!executable) return false;
                if(s=="EnshroudedModInitialize") facts.initialize=true; else facts.configure=true;
            }
        }
    }
    return true;
}
struct LoadPlan {
    std::vector<Package> ordered;
    std::vector<std::unique_ptr<FileHandle>> locks;
};
static bool systemImport(const std::string& name) {
    if(name.rfind("api-ms-win-",0)==0 || name.rfind("ext-ms-win-",0)==0) return true;
    std::vector<wchar_t> system(32768); const auto n=GetSystemDirectoryW(system.data(),32768);
    if(!n || n>=32768) return false;
    std::error_code ec;
    return std::filesystem::is_regular_file(std::filesystem::path(system.data())/std::filesystem::u8path(name),ec) && !ec;
}
static bool makeLoadPlan(const std::set<std::string>& desired,LoadPlan& plan,std::string& error,bool metadataOnly=false) {
    plan.ordered.clear(); plan.locks.clear(); error.clear();
    std::map<std::string,Package> packages;
    std::set<std::string> visiting,done,physical,names;
    std::function<bool(const std::string&,size_t)> visit;
    visit=[&](const std::string& id,size_t depth) {
        if(done.count(id)) return true;
        if(depth>64 || visiting.count(id) || packages.size()>=MaxMods) {error="Cycle ou trop de dependances : "+id; return false;}
        Package item; if(!readPackage(id,item,error)) return false;
        if(item.mod && !desired.count(id)) {error="Mod requis mais desactive : "+id; return false;}
        if(!item.mod && desired.count(id)) {error="Une bibliotheque ne s'active pas seule : "+id; return false;}
        if(!metadataOnly) {
        auto lock=std::make_unique<FileHandle>(CreateFileW(item.path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
        BY_HANDLE_FILE_INFORMATION info{};
        if(lock->value==INVALID_HANDLE_VALUE || !GetFileInformationByHandle(lock->value,&info)) {error="DLL inaccessible : "+id; return false;}
        const auto identity=std::to_string(info.dwVolumeSerialNumber)+":"+std::to_string(info.nFileIndexHigh)+":"+std::to_string(info.nFileIndexLow);
        const auto basename=lowerAscii(item.path.filename().u8string());
        if(!physical.insert(identity).second || !names.insert(basename).second) {error="DLL en double ou nom ambigu : "+id; return false;}
        if(GetModuleHandleW(item.path.filename().c_str())) {error="DLL deja chargee hors de ce profil : "+id; return false;}
        plan.locks.push_back(std::move(lock));
        }
        packages.emplace(id,item); visiting.insert(id);
        for(const auto& dep:item.dependencies) {
            if(!visit(dep.first,depth+1)) return false;
            if(packages.at(dep.first).version!=dep.second) {error="Version requise differente : "+dep.first+" @"+dep.second; return false;}
        }
        if(!metadataOnly) {
        std::string bytes; PeFacts facts;
        if(!boundedRead(item.path,128*1024*1024,bytes) || !inspectPe(bytes,facts) || facts.initialize!=item.mod || (!item.settings.empty() && !facts.configure)) {error="DLL x64/contrat incompatible : "+id; return false;}
        for(const auto& imported:facts.imports) {
            bool declared=false;
            for(const auto& dep:item.dependencies) if(lowerAscii(packages.at(dep.first).path.filename().u8string())==imported) declared=true;
            std::error_code ec;
            const bool local=std::filesystem::exists(item.path.parent_path()/std::filesystem::u8path(imported),ec);
            if(!declared && (local || ec || !systemImport(imported))) {error="Dependance non declaree : "+id+" -> "+imported; return false;}
        }
        }
        visiting.erase(id); done.insert(id); plan.ordered.push_back(std::move(item)); return true;
    };
    for(const auto& id:desired) if(!visit(id,0)) return false;
    return true;
}
