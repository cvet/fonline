# FOnline ThirdParty pruning notes

Upstream: Effekseer 1.80.5, tag `1805`, commit
`332525b2086e4c97f0cdbdd6c2f2b07e26f754d2`.

This is a source-only vendor slice for two independent consumers:

- FOnline runtime and the headless project compiler use the C++ CPU simulation
  core and managed `EffekseerCore` sources;
- the standalone Windows win64 Editor build uses the managed
  Editor/Core/CoreGUI, native Viewer, material editor/compilers, Direct3D 11 and
  OpenGL renderer support, OpenSoundMixer, IPC/OpenGL helpers, resources, and
  their required third-party sources.

No Editor UI, Viewer, material tool, or stock graphics backend is linked into an
FOnline runtime target. The Editor is absent from the engine CMake graph; its
dedicated script writes every generated artifact outside this source tree.

## Retained upstream areas

- top-level CMake support, `cmake/`, `Script/`, English-only `ResourceData/`,
  licenses, and `Dev/release/` Editor resources;
- `Dev/Cpp/Effekseer/` plus the native Editor support directories present in
  this tree (`3rdParty`, `EditorCommon`, material components,
  `EffekseerRendererCommon`, `EffekseerRendererDX11`, `EffekseerRendererGL`,
  `EffekseerSoundOSMixer`, `EffekseerToolRuntime`, `IPC`, `OpenGLHelper`, and
  `Viewer`);
- `Dev/Editor/Effekseer/`, `EffekseerCore/`, and `EffekseerCoreGUI/`.

The isolated Editor build reuses the engine-owned `ThirdParty/zlib`,
`ThirdParty/libpng`, `ThirdParty/ogg`, and `ThirdParty/Vorbis` source trees.
`BuildTools/EffekseerEditor/build.ps1` passes their parent directory through
`FONLINE_SHARED_THIRD_PARTY_DIR`; the Effekseer vendor does not carry second
copies. OpenSoundMixer retains its wrapper/decoder sources but compiles the
shared Ogg and Vorbis sources into its isolated library.

Effekseer's imgui is deliberately retained. It is the upstream docking branch
with the multi-viewport `ImGuiPlatformIO` ABI required by the bundled Viewer and
material editor. The engine-owned standard imgui branch does not expose that
ABI and is therefore not an interchangeable duplicate.

## Source-only pruning

Upstream application binaries, libraries, debug symbols, and cooked effect
fixtures are not retained. In particular, the restored examples/tests were
pruned of the DXSDK `d3dx11*.lib` files, imgui's `glfw3.lib`, stb's
`oversample.exe`, and `Viewer/Resource/test.efk`. This vendor contains no
`.efk`, `.efkefc`, `.dll`, `.exe`, `.lib`, `.a`, `.so`, `.dylib`, or `.pdb`
files.

The unsupported native renderer/sound families and unrelated upstream
applications, top-level examples/tests, release packages, downloads,
documentation, and repository automation remain pruned. The retained native
third-party dependencies are source-only slices as well:

- `libgd/` is omitted because GIF recording is disabled;
- imgui and imgui-node-editor examples, documentation, repository automation,
  and bundled duplicate dependencies are omitted;
- the nested zlib and libpng trees and OpenSoundMixer's nested Ogg/Vorbis trees
  are omitted in favor of the engine-owned sources;
- GLFW, spdlog, nativefiledialog, and OpenSoundMixer examples, tests,
  documentation, and repository automation are omitted where they are not
  required by the isolated Editor build;
- stb retains only its readme and the `stb_image.h` / `stb_image_write.h`
  headers consumed by the Viewer;
- every localization except `Dev/release/resources/languages/en/` is omitted;
  the Editor and Material Editor are forced to English, and stale saved language
  selections fall back to English;
- the 17.4 MiB multilingual `SourceHanSans-Normal.ttc` collection is replaced
  by the 17.4 KiB `FOnlineEffekseerLatin-Regular.otf` subset. It contains only
  printable Basic Latin (`U+0020-007E`) and is renamed to avoid the upstream OFL
  Reserved Font Name.

When refreshing the vendor, start from the pinned commit, restore only the
areas above and their actual source/resource dependencies, remove generated
`bin`/`obj` output, then repeat the extension audit before committing.

## Local adaptations to preserve

- Editor **Save** and **Save As** are source-first and accept only normalized
  `.efkproj` XML; `.efkefc` is import-only.
- The headless compiler uses `FONLINE_HEADLESS` to exclude GUI-only scripting,
  package, exporter, and JSON dependencies while sharing the same Core source.
- Native and managed builds route generated output outside the vendor and run
  warning-clean under the FOnline tool target.
- Preserve the shared zlib/libpng/Ogg/Vorbis wiring and the minimal install
  staging used by the pruned common zlib/libpng source trees.
- GIF recording is optional upstream and disabled by the FOnline bundle so the
  unused libgd build is omitted.
- The Editor and Material Editor use the bundled Latin-only Source Han subset;
  preserve the English-only `(FOnline Patch)` markers in `Application.cs`,
  `Localization.cs`, `GUI/Manager.cs`, `Config.cpp`, and
  `effekseerMaterialEditor.cpp`.
- The subset is derived from face 0 of upstream `SourceHanSans-Normal.ttc` at
  commit `332525b2086e4c97f0cdbdd6c2f2b07e26f754d2` (source SHA-256
  `6990AD2B950CEB017CE29EA377415FC7FB79BC5B3152B9BBFF9202AD774A6972`)
  with FontTools 4.63.0. Preserve only `U+0020-007E`, the English name records
  and SIL OFL metadata, and rename both OpenType and CFF family
  names to `FOnline Effekseer Latin` when regenerating it. The resulting file's
  SHA-256 is
  `4EDA5E029C07954FE43508B615D42191D9399D109FA91EB561587058725E8365`.
- Existing `(FOnline Patch)` markers, strict zlib behavior, source-first save
  behavior, and warning fixes must be re-audited rather than overwritten during
  an upstream refresh.
