from pathlib import Path
import sys

import pytest

buildtools_dir = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(buildtools_dir))
import buildtools  # noqa: E402


@pytest.mark.parametrize('previous', ['_wasmglue_asm_id_host_flags', '_wasmglue_asm_id_zlib_dispatch_o0'])
def test_combined_browser_cache_invalidates_both_previous_single_patch_revisions(previous):
    combined = buildtools.MONO_BROWSER_SUBSET_MARKER_SUFFIX
    assert combined != buildtools.MONO_SUBSET_MARKER_SUFFIX + previous
    assert combined.endswith('_wasmglue_asm_id_host_flags_zlib_dispatch_o0')
    assert callable(buildtools.patch_runtime_browser_host_compile_options)
    assert callable(buildtools.patch_runtime_browser_zlib_dispatch)
    stage = (buildtools_dir / 'cmake/stages/ThirdParty.cmake').read_text(encoding='utf-8')
    assert f'READY_${{FO_MONO_RUNTIME_VERSION}}_${{FO_MONO_TRIPLET}}{combined})' in stage
