#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace tsvitch::update {
inline bool validNro(const std::filesystem::path& file, uint64_t expected) {
    std::error_code ec;
    if (expected < 0x80 || std::filesystem::file_size(file, ec) != expected || ec) return false;
    std::array<unsigned char, 0x80> header{};
    std::ifstream input(file, std::ios::binary);
    if (!input.read(reinterpret_cast<char*>(header.data()), header.size())) return false;
    if (std::string(reinterpret_cast<char*>(header.data()+0x10),4) != "NRO0") return false;
    uint32_t codeSize=0;
    for (int i=0;i<4;++i) codeSize |= uint32_t(header[0x18+i]) << (i*8);
    if (codeSize < header.size() || codeSize > expected || expected-codeSize < 0x38) return false;
    input.seekg(codeSize);
    std::array<unsigned char,0x38> assets{};
    if (!input.read(reinterpret_cast<char*>(assets.data()),assets.size()) ||
        std::string(reinterpret_cast<char*>(assets.data()),4)!="ASET") return false;
    for (int pair=0;pair<3;++pair) {
        uint64_t offset=0,size=0;
        for (int i=0;i<8;++i) {
            offset |= uint64_t(assets[8+pair*16+i]) << (i*8);
            size |= uint64_t(assets[16+pair*16+i]) << (i*8);
        }
        if (!size || offset<assets.size() || offset>expected-codeSize || size>expected-codeSize-offset) return false;
    }
    return true;
}

// Called only after closing RomFS. A failed replacement restores the working NRO.
inline bool replaceNro(const std::filesystem::path& path, std::string& error) {
    const auto staged=path.string()+".new", backup=path.string()+".bak";
    std::error_code ec;
    if (!std::filesystem::is_regular_file(staged,ec)) {error="No staged update";return false;}
    std::filesystem::remove(backup,ec);
    if (ec) {error="Cannot remove old backup: "+ec.message();return false;}
    std::filesystem::rename(path,backup,ec);
    if (ec) {error="Cannot back up application: "+ec.message();return false;}
    std::filesystem::rename(staged,path,ec);
    if (!ec) return true;
    error="Cannot install update: "+ec.message();
    std::filesystem::rename(backup,path,ec);
    if (ec) error+="; restore backup manually: "+ec.message();
    return false;
}
}
