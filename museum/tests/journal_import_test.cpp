#include "game/journal_import.h"
#include "core/logging.h"
#include <cstdio>
#include <string>
namespace {
int failures=0;
void check(bool value,const char* message) { if (!value) { ++failures; std::printf("FAIL: %s\n",message); } }
void write(const std::wstring& path,const char* text) {
    FILE* file=nullptr; _wfopen_s(&file,path.c_str(),L"wb");
    if (!file) { ++failures; return; }
    std::fputs(text,file); std::fclose(file);
}
std::string read(const std::wstring& path) {
    FILE* file=nullptr; _wfopen_s(&file,path.c_str(),L"rb");
    if (!file) return {};
    char buffer[128]={}; const size_t count=std::fread(buffer,1,sizeof(buffer)-1,file); std::fclose(file);
    return {buffer,count};
}
}
int wmain(int argc,wchar_t** argv) {
    if (argc!=2) return 2;
    const std::wstring root=argv[1], source=root+L"\\reference", target=root+L"\\museum";
    CreateDirectoryW(source.c_str(),nullptr); CreateDirectoryW(target.c_str(),nullptr);
    integration::logInit((root+L"\\import.log").c_str());
    write(source+L"\\tq-uniq-items.jsonl","reference campaign\n");
    write(source+L"\\tq-uniq-items-custom.jsonl","reference custom\n");
    write(source+L"\\unrelated.jsonl","unrelated\n");
    write(target+L"\\tq-uniq-items.jsonl","existing Museum\n");
    check(museum::game::copyMissingJournals(source.c_str(),target.c_str())==1,"copy only missing named journals");
    check(read(target+L"\\tq-uniq-items.jsonl")=="existing Museum\n","existing Museum collection never overwritten");
    check(read(target+L"\\tq-uniq-items-custom.jsonl")=="reference custom\n","import is byte-exact");
    check(read(source+L"\\tq-uniq-items.jsonl")=="reference campaign\n","reference campaign unchanged");
    check(read(source+L"\\tq-uniq-items-custom.jsonl")=="reference custom\n","reference custom unchanged");
    check(read(target+L"\\unrelated.jsonl").empty(),"unrelated files not imported");
    check(museum::game::copyMissingJournals(source.c_str(),target.c_str())==0,"repeat import never replaces data");
    check(museum::game::copyMissingJournals(source.c_str(),source.c_str())==0,"same directory ignored");
    integration::logShutdown();
    std::printf("Journal import: 8 checks, %d failures\n",failures);
    return failures ? 1 : 0;
}
