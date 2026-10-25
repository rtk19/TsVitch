#include "utils/update_files.hpp"
#include <cassert>
#include <iostream>
#include <vector>
using namespace tsvitch;
namespace fs=std::filesystem;
static void write(const fs::path& p,const std::string& s){std::ofstream(p,std::ios::binary)<<s;}
static std::string read(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){
    assert(argc==2);fs::path root=argv[1];fs::create_directories(root);
    const auto app=root/"TsVitch.nro";const auto staged=app.string()+".new",backup=app.string()+".bak";
    std::string nro(512,'\0');nro.replace(0x10,4,"NRO0");nro[0x19]=1;nro.replace(256,4,"ASET");
    for(int pair=0;pair<3;++pair){nro[256+8+pair*16]=64+pair*32;nro[256+16+pair*16]=16;}
    write(staged,nro);assert(update::validNro(staged,nro.size()));
    assert(!update::validNro(staged,nro.size()+1));assert(!update::validNro(staged,128));
    auto invalid=nro;invalid[0x10]='X';write(staged,invalid);assert(!update::validNro(staged,invalid.size()));
    invalid=nro;invalid[0x19]=3;write(staged,invalid);assert(!update::validNro(staged,invalid.size()));
    invalid=nro;invalid[256+8]=255;write(staged,invalid);assert(!update::validNro(staged,invalid.size()));
    invalid=nro;invalid[256+16]=0;write(staged,invalid);assert(!update::validNro(staged,invalid.size()));
    write(app,"working application");write(staged,nro);std::string error;
    assert(update::replaceNro(app,error));assert(read(app)==nro);assert(read(backup)=="working application");assert(!fs::exists(staged));
    assert(!update::replaceNro(app,error));assert(read(app)==nro);
    // A backup that cannot be replaced must not remove the running application.
    fs::remove(backup);fs::create_directory(backup);write(fs::path(backup)/"keep","backup");write(staged,"next");
    assert(!update::replaceNro(app,error));assert(read(app)==nro);assert(read(staged)=="next");
    fs::remove_all(backup);fs::remove(app);
    assert(!update::replaceNro(app,error));assert(read(staged)=="next");
    std::cout<<"Update NRO bounds, staged installation and failure preservation passed\n";
}
