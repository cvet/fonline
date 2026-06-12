FOnline ThirdParty pruning notes

This vendored copy is intentionally trimmed for the engine build. When updating
from upstream, remove these paths again after copying the new version.

Removed paths:
- build_overrides/
- External/
- gtests/
- kokoro/
- ndk_test/
- StandAlone/
- .clang-format
- .gitattributes
- .gitignore
- .gn
- .mailmap
- _config.yml
- Android.mk
- BUILD.gn
- DEPS
- gen_extension_headers.py
- known_good.json
- known_good_khr.json
- license-checker.cfg
- standalone.gclient
- update_glslang_sources.py

Retained files: StandAlone/DirStackFileIncluder.h is kept because the upstream
glslang C interface source includes it during the engine build.
(FOnline Patch)
