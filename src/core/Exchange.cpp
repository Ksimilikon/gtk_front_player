#include "core/Exchange.hpp"

#include <ctime>
#include <fstream>
#include <iterator>

#include "core/Core.hpp"
#include "util/Paths.hpp"

namespace dp::exchange {

std::string default_file_name() {
    return "dream_player_" + std::to_string(static_cast<long long>(std::time(nullptr))) + kExtension;
}

Result export_to_file(const Selection& what, const Flags& f, const std::string& file) {
    core::LongOp op;
    Result r;
    uint8_t* ptr = nullptr;
    size_t len = 0;
    switch (what.kind) {
        case Selection::Library:
            r.status = cdc_pack_library(CDC_VERSION_LATEST, f.audio, f.names, f.covers, &ptr, &len);
            break;
        case Selection::Playlists: {
            core::CStrings names(what.playlists);
            r.status = cdc_pack_playlists(CDC_VERSION_LATEST, names.data(), names.size(), f.audio, f.names, f.covers,
                                          &ptr, &len);
            break;
        }
        case Selection::Tracks:
            r.status = cdc_pack_tracks(CDC_VERSION_LATEST, what.tracks.empty() ? nullptr : what.tracks.data(),
                                       what.tracks.size(), f.audio, f.names, f.covers, &ptr, &len);
            break;
    }
    if (r.status != 0) return r;
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(ptr), static_cast<std::streamsize>(len));
    out.close();
    cdc_free_bytes(ptr, len);
    if (!out) {
        r.io_error = true;
        r.message = file;
        return r;
    }
    r.bytes = len;
    return r;
}

Result import_file(const std::string& file, bool force, const std::string& target_dir) {
    Result r;
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        r.io_error = true;
        r.message = file;
        return r;
    }
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    r.bytes = bytes.size();

    core::LongOp op;
    ImportSession* session = nullptr;
    FFIImportFiles files{};
    r.status = cdc_import_prepare(bytes.data(), bytes.size(), force, &session, &files);
    if (r.status != 0) return r;

    std::vector<std::string> written(files.len);
    std::vector<const char*> paths(files.len, nullptr);
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(target_dir, ec);
    for (size_t i = 0; i < files.len; ++i) {
        const FFIImportFile& f = files.ptr[i];
        const fs::path dest = paths::unique_path(target_dir, core::str(f.name));
        std::ofstream out(dest, std::ios::binary);
        out.write(reinterpret_cast<const char*>(f.data), static_cast<std::streamsize>(f.data_len));
        out.close();
        if (!out) {
            ++r.failed;
            continue;
        }
        written[i] = dest.string();
        paths[i] = written[i].c_str();
        (f.kind == FFI_IMPORT_SONG ? r.songs : r.covers)++;
    }
    r.status = cdc_import_commit(session, paths.empty() ? nullptr : paths.data(), paths.size());
    if (r.status != 0) {
        // the db is unchanged: the written files are ours to delete.
        for (const auto& p : written)
            if (!p.empty()) fs::remove(p, ec);
    }
    return r;
}

}  // namespace dp::exchange
