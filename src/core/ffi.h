// C declarations of the rs_dream_player core ABI (api crate, feature
// `modding`). Hand-written from the Rust sources: the core is in beta and its
// FFI may change — keep this file in sync with ../rs_dream_player/api/src.
//
// Every function returns a status (`0` = ok); results go through
// out-parameters. Owned buffers are freed with the paired `*_free_*`.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ───────────────────────────── init / misc ─────────────────────────────────
uint32_t init(const char* cache_dir, const char* config_dir, const char* data_dir,
              const char* music_dir, bool need_db);
uint32_t core_get_dir(uint32_t kind, char** out);  // FFIIdxStatus
uint64_t core_panic_count(void);
char* core_last_panic(void);  // free with idx_free_string

// ───────────────────────────── idx (library) ───────────────────────────────
typedef struct {
    uint64_t duration_s;
    uint32_t sample_rate;
    uint32_t bitrate;
    int64_t listen_count;
    char* cover_art;  // maybe null
} FFITrackMetadataParams;

typedef struct {
    bool has_id;
    int64_t id;
    char* path;  // maybe null
    char* title;
    char** artists;
    size_t artists_len;
    char* album;  // maybe null
    char** genres;
    size_t genres_len;
    bool has_params;
    FFITrackMetadataParams params;
    float volume;
    bool invalid;
    char* color;       // maybe null
    char* user_label;  // maybe null
    bool ignore_orphan;
} FFITrackMetadata;

typedef struct {
    FFITrackMetadata* ptr;
    size_t len;
} FFITracksList;

typedef struct {
    char** ptr;
    size_t len;
} FFIStringList;

typedef struct {
    char* name;  // null = anonymous
    int64_t* tracks;
    size_t tracks_len;
    char* cover_art;  // maybe null
    bool has_updated_at;
    uint64_t updated_at;
    bool has_created_at;
    uint64_t created_at;
} FFIPlaylist;

typedef struct {
    FFIPlaylist* ptr;
    size_t len;
} FFIPlaylistsList;

uint32_t idx_get_playlists(FFIPlaylistsList* out);
uint32_t idx_get_tracks(FFITracksList* out);
uint32_t idx_get_orphan_tracks(uint32_t filter, FFITracksList* out);
uint32_t idx_set_track_ignore_orphan(int64_t id, bool ignore);
uint32_t idx_get_playlist_tracks(int64_t id_playlist, FFITracksList* out);
uint32_t idx_get_track(int64_t id_track, const char* path, FFITrackMetadata* out);
uint32_t idx_get_track_cover(int64_t id_track, const char* path, uint8_t** out_ptr, size_t* out_len);
uint32_t idx_index_dir(const char* dir);
uint32_t idx_set_track_meta(int64_t id, const char* title, const char* const* artists, size_t artists_len,
                            const char* album, const char* const* genres, size_t genres_len);
uint32_t idx_set_track_album(int64_t id, const char* album);
uint32_t idx_set_track_genres(int64_t id, const char* const* genres, size_t genres_len);
uint32_t idx_set_track_cover(int64_t id, const char* cover_path);
uint32_t idx_embed_track_cover(int64_t id, const char* image_path);
uint32_t idx_set_track_file_changed(int64_t id);
uint32_t idx_set_track_color(int64_t id, const char* color);
uint32_t idx_set_track_label(int64_t id, const char* label);
uint32_t idx_set_track_invalid(int64_t id, bool invalid);
uint32_t idx_set_tracks_invalid(const int64_t* ids, size_t len, bool invalid);
uint32_t idx_add_tracks(const char* const* paths, size_t len, uint64_t* out);
uint32_t idx_set_track_volume(int64_t id, float volume);
uint32_t idx_set_track_path(int64_t id, const char* path);
uint32_t idx_remove_track(int64_t id);
uint32_t idx_remove_invalid_tracks(uint64_t* out);
uint32_t idx_save_playlist(const char* name, const int64_t* track_ids, size_t track_ids_len, const char* cover);
uint32_t idx_set_playlist_cover(const char* name, const char* cover);
uint32_t idx_remove_playlist(const char* name);

void idx_free_string(char* s);
void idx_free_track_metadata(FFITrackMetadata meta);
void idx_free_tracks_list(FFITracksList list);
void idx_free_playlists_list(FFIPlaylistsList list);
void idx_free_bytes(uint8_t* ptr, size_t len);

// ───────────────────────────── engine (playback) ───────────────────────────
typedef void (*OnTrackEnd)(void);  // may be null; called on the audio thread

uint32_t engine_add_track_id(int64_t id_track, OnTrackEnd on_end);
uint32_t engine_add_track_path(const char* path, OnTrackEnd on_end);
uint32_t engine_play_pause(void);
uint32_t engine_play(void);
uint32_t engine_pause(void);
uint32_t engine_stop(void);
uint32_t engine_set_speed(float speed);
uint32_t engine_set_seek(uint64_t seek_sec);
uint32_t engine_get_seek(uint64_t* out);
uint32_t engine_get_duration(uint64_t* out);
uint32_t engine_is_pause(bool* out);
uint32_t engine_is_empty(bool* out);
uint32_t engine_set_volume_track(float volume);
uint32_t engine_set_volume_master(float volume);
uint32_t engine_get_volume_track(float* out);
uint32_t engine_get_volume_master(float* out);
uint32_t engine_get_output_devices(FFIStringList* out);
uint32_t engine_get_output_device(char** out);
uint32_t engine_set_output_device(const char* name);
void engine_free_output_devices(FFIStringList list);

// ───────────────────────────── cdc (packets) ───────────────────────────────
enum { CDC_VERSION_LATEST = 4 };  // codec::Version::V0_13

typedef enum : uint32_t {
    FFI_IMPORT_SONG = 1,
    FFI_IMPORT_COVER = 2,
    FFI_IMPORT_SONG_COVER = 3,
} FFIImportFileKind;

typedef struct {
    FFIImportFileKind kind;
    const char* name;
    const uint8_t* data;
    size_t data_len;
} FFIImportFile;

typedef struct {
    const FFIImportFile* ptr;
    size_t len;
} FFIImportFiles;

typedef struct ImportSession ImportSession;

uint32_t cdc_pack_library(uint32_t version, bool with_audio, bool with_names, bool with_covers,
                          uint8_t** out_ptr, size_t* out_len);
uint32_t cdc_pack_tracks(uint32_t version, const int64_t* ids, size_t ids_len, bool with_audio,
                         bool with_names, bool with_covers, uint8_t** out_ptr, size_t* out_len);
uint32_t cdc_pack_playlists(uint32_t version, const char* const* names, size_t names_len, bool with_audio,
                            bool with_names, bool with_covers, uint8_t** out_ptr, size_t* out_len);
uint32_t cdc_import_prepare(const uint8_t* bytes, size_t len, bool force, ImportSession** out_session,
                            FFIImportFiles* out_files);
uint32_t cdc_import_commit(ImportSession* session, const char* const* paths, size_t paths_len);
void cdc_import_cancel(ImportSession* session);
void cdc_free_bytes(uint8_t* ptr, size_t len);

// ───────────────────────────── con (pairing) ───────────────────────────────
typedef struct {
    int64_t id;
    uint8_t pub_key[32];
    char* name;
} FFIConPaired;

typedef struct {
    FFIConPaired* ptr;
    size_t len;
} FFIConPairedList;

uint32_t con_get_pub_key(uint8_t (*out)[32]);
uint32_t con_add(uint64_t* out);
uint32_t con_remove(uint64_t id);
uint32_t con_connect(uint64_t id);
uint32_t con_disconnect(uint64_t id);
uint32_t con_get_paired(FFIConPairedList* out);
uint32_t con_rename(int64_t paired_id, const char* name);
uint32_t con_forget(int64_t paired_id);
void con_free_paired_list(FFIConPairedList list);

// ───────────────────────────── sync ────────────────────────────────────────
typedef enum : uint32_t {
    FFI_SYNC_EV_NONE = 0,
    FFI_SYNC_EV_PAIRED = 1,
    FFI_SYNC_EV_IMPORT = 2,
    FFI_SYNC_EV_SENT = 3,
    FFI_SYNC_EV_RECEIVED = 4,
    FFI_SYNC_EV_PEER_REJECTED = 5,
    FFI_SYNC_EV_DONE = 6,
    FFI_SYNC_EV_FAILED = 7,
} FFISyncEventKind;

typedef struct {
    FFISyncEventKind kind;
    uint32_t status;
    bool by_peer;
    int64_t paired_id;
    uint32_t songs;
    uint32_t playlists;
} FFISyncEvent;

typedef struct {
    FFIImportFileKind kind;
    const char* name;
    const uint8_t* data;
    size_t data_len;
    const char* replace;  // null — write anywhere
} FFISyncFile;

typedef struct {
    const FFISyncFile* ptr;
    size_t len;
} FFISyncFiles;

typedef struct FFISyncSession FFISyncSession;

uint32_t sync_new(uint64_t con, bool host, FFISyncSession** out);
uint32_t sync_new_send(uint64_t con, bool host, uint32_t what, const char* const* names, size_t names_len,
                       const int64_t* ids, size_t ids_len, bool with_audio, bool with_names, bool with_covers,
                       FFISyncSession** out);
uint32_t sync_new_receive(uint64_t con, bool host, bool force, FFISyncSession** out);
void sync_free(FFISyncSession* session);
uint32_t sync_feed(FFISyncSession* session, const uint8_t* data, size_t len);
uint32_t sync_take_output(FFISyncSession* session, uint8_t** out_ptr, size_t* out_len);
uint32_t sync_poll_event(FFISyncSession* session, FFISyncEvent* out);
bool sync_finished(FFISyncSession* session);
uint32_t sync_import_files(FFISyncSession* session, FFISyncFiles* out);
uint32_t sync_import_done(FFISyncSession* session, const char* const* paths, size_t paths_len);
uint32_t sync_cancel(FFISyncSession* session);
void sync_free_bytes(uint8_t* ptr, size_t len);

// ───────────────────────────── mods (pre-alpha) ────────────────────────────
// ModStatus codes (u32): 0 ok, 1 NotFound, 2 LoadFailed, 3 SymbolMissing,
// 4 AbiMismatch, 5 InvalidMeta, 6 AlreadyAdded, 7 AlreadyRunning,
// 8 NotRunning, 9 UiClosed, 10 InvalidArg, 11 Unsupported, 12 Io,
// 13 NullPointer, 14 NotInitialized, 100 InitFailed, 101 StopFailed,
// 102 ActionFailed, 103 Crashed, 104 Panic, UINT32_MAX Unknown.

typedef enum : uint32_t {
    FFI_MOD_STOPPED = 0,
    FFI_MOD_RUNNING = 1,
    FFI_MOD_FAILED = 2,
} FFIModState;

typedef struct {
    char* path;
    char* name;
    char* version;
    char* author;       // maybe null
    char* description;  // maybe null
    FFIModState state;
    bool is_static;
    uint32_t error;
    char* error_msg;  // maybe null
} FFIModInfo;

typedef struct {
    FFIModInfo* ptr;
    size_t len;
} FFIModsList;

enum { UI_PLACE_MENU = 0, UI_PLACE_PLAYLIST = 1, UI_PLACE_SONG = 2 };

typedef uint32_t (*ModActionFn)(void* ctx, uint64_t target);

// mod_sys::ui::Element — `#[repr(C, u32)]` enum.
typedef enum : uint32_t {
    MOD_EL_TEXT = 0,
    MOD_EL_IMAGE = 1,
    MOD_EL_INPUT = 2,
    MOD_EL_BUTTON = 3,
    MOD_EL_LIST = 4,
    MOD_EL_BOX = 5,
    MOD_EL_INFO = 6,
    MOD_EL_TOGGLE = 7,
    MOD_EL_SELECT = 8,
} ModElementKind;

typedef struct ModElement ModElement;
struct ModElement {
    ModElementKind kind;
    union {
        struct { const char* text; } text;
        struct { const uint8_t* data; size_t len; } image;
        struct { const char* hint; char* buf; size_t cap; } input;
        struct { const char* title; ModActionFn action; void* ctx; } button;
        struct { const char* const* items; size_t len; ModActionFn on_select; void* ctx; } list;
        struct { const ModElement* children; size_t len; bool horizontal; } box;
        struct { const char* const* strings; size_t len; } info;
        struct { const char* title; bool on; ModActionFn action; void* ctx; } toggle;
        struct { const char* title; const char* const* items; size_t len; size_t selected;
                 ModActionFn on_select; void* ctx; } select;
    };
};

typedef struct {
    const char* title;
    const ModElement* elements;
    size_t len;
} ModTab;

typedef void (*FFIModErrorListener)(const char* path, uint32_t code, const char* msg);

uint32_t mods_get_list(FFIModsList* out);
void mods_free_list(FFIModsList list);
uint32_t mods_rescan(void);
uint32_t mods_add(const char* path);
uint32_t mods_remove(const char* path);
uint32_t mods_start(const char* path);
uint32_t mods_stop(const char* path);
uint32_t mods_start_all(void);
uint32_t mods_stop_all(void);
uint32_t mods_get_actions(const char* path, uint32_t place, FFIStringList* out);
void mods_free_strings(FFIStringList list);
uint32_t mods_call_action(const char* path, uint32_t place, uint32_t index, uint64_t target);
uint32_t mods_get_tab_count(const char* path, uint32_t* out);
uint32_t mods_get_tab(const char* path, uint32_t id, const ModTab** out);
uint32_t mods_activate(const char* path, const ModElement* element, uint64_t target);
uint32_t mods_set_error_listener(FFIModErrorListener listener);

#ifdef __cplusplus
}
#endif
