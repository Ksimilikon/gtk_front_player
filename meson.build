project('dream_player_gtk', 'cpp',
  version : '0.1.0',
  license : 'GPL-3.0-or-later',
  meson_version : '>= 1.1',
  default_options : ['cpp_std=c++20', 'warning_level=2', 'buildtype=debugoptimized'])

app_id = 'dev.dreamplayer.gtk'

gtkmm = dependency('gtkmm-4.0', version : '>= 4.10')
giomm = dependency('giomm-2.68')
threads = dependency('threads')

# ─── the core (rs_dream_player) ───────────────────────────────────────────────
# built by cargo as a cdylib with the mod system enabled; the C ABI is
# declared in src/core/ffi.h.
core_dir = get_option('core_dir')
if not core_dir.startswith('/')
  core_dir = meson.project_source_root() / core_dir
endif
cargo = find_program('cargo')
core_profile = get_option('core_profile')
core_target_dir = meson.project_build_root() / 'core-target'

core_version = run_command('sh', '-c',
  'sed -n "s/^version *= *\\"\\(.*\\)\\"/\\1/p" "' + core_dir / 'Cargo.toml' + '" | head -1',
  check : false).stdout().strip()
if core_version == ''
  core_version = 'unknown'
endif

core_lib = custom_target('dream_player_core',
  output : 'libdream_player.so',
  command : [files('build-aux/build_core.sh'), cargo, core_dir, core_target_dir,
             core_profile, '@OUTPUT@'],
  console : true,
  build_always_stale : true,
  install : true,
  install_dir : get_option('libdir'))

# ─── the app ──────────────────────────────────────────────────────────────────
datadir = get_option('prefix') / get_option('datadir') / meson.project_name()

conf = configuration_data()
conf.set_quoted('DP_APP_ID', app_id)
conf.set_quoted('DP_APP_VERSION', meson.project_version())
conf.set_quoted('DP_CORE_VERSION', core_version)
conf.set_quoted('DP_INSTALL_DATA_DIR', datadir)
conf.set_quoted('DP_SOURCE_DATA_DIR', meson.project_source_root() / 'data')
configure_file(output : 'config.h', configuration : conf)

sources = files(
  'src/main.cpp',
  'src/app/Application.cpp',
  'src/core/Core.cpp',
  'src/core/Library.cpp',
  'src/core/Player.cpp',
  'src/core/Covers.cpp',
  'src/core/Mods.cpp',
  'src/core/SyncWorker.cpp',
  'src/core/Exchange.cpp',
  'src/dbus/Mpris.cpp',
  'src/i18n/I18n.cpp',
  'src/theme/Theme.cpp',
  'src/util/Log.cpp',
  'src/util/Paths.cpp',
  'src/util/Settings.cpp',
  'src/util/Format.cpp',
  'src/ui/MainWindow.cpp',
  'src/ui/TabHost.cpp',
  'src/ui/Menus.cpp',
  'src/ui/widgets/WaveView.cpp',
  'src/ui/widgets/SeekBar.cpp',
  'src/ui/widgets/PlayerPanel.cpp',
  'src/ui/widgets/Controls.cpp',
  'src/ui/widgets/Common.cpp',
  'src/ui/widgets/TrackRow.cpp',
  'src/ui/tabs/PlaylistsTab.cpp',
  'src/ui/tabs/PlaylistTab.cpp',
  'src/ui/tabs/EditorTab.cpp',
  'src/ui/tabs/SongTab.cpp',
  'src/ui/tabs/SettingsTab.cpp',
  'src/ui/tabs/SyncTab.cpp',
  'src/ui/tabs/ImportTab.cpp',
  'src/ui/tabs/ExportTab.cpp',
  'src/ui/tabs/LogsTab.cpp',
  'src/ui/tabs/ModsTab.cpp',
  'src/ui/tabs/ModPageTab.cpp',
  'src/ui/tabs/AboutTab.cpp',
)

executable('dream_player_gtk', sources, core_lib,
  include_directories : include_directories('src', '.'),
  dependencies : [gtkmm, giomm, threads],
  link_with : core_lib,
  build_rpath : meson.project_build_root(),
  install_rpath : get_option('prefix') / get_option('libdir'),
  install : true)

install_subdir('data/lang', install_dir : datadir)
install_subdir('data/themes', install_dir : datadir)
install_data('data/LICENSE-app.txt', 'data/LICENSE-core.txt', install_dir : datadir)
install_data('data/dev.dreamplayer.gtk.desktop',
  install_dir : get_option('datadir') / 'applications')
