from __future__ import annotations

import pytest

import _timeouts


def test_default_timeout_is_generous_and_never_below_the_old_fixed_limits() -> None:
    assert _timeouts.read_cmake_timeout({}) == 600.0
    assert _timeouts.read_cmake_timeout({_timeouts.CMAKE_TIMEOUT_ENV: "  "}) == 600.0
    assert _timeouts.DEFAULT_CMAKE_TIMEOUT_SECONDS >= 240


def test_environment_override_sets_the_timeout() -> None:
    assert _timeouts.read_cmake_timeout({_timeouts.CMAKE_TIMEOUT_ENV: "1200"}) == 1200.0
    assert _timeouts.read_cmake_timeout({_timeouts.CMAKE_TIMEOUT_ENV: "7.5"}) == 7.5


@pytest.mark.parametrize("value", ["abc", "0", "-5", "nan", "inf"])
def test_invalid_override_fails_instead_of_falling_back(value: str) -> None:
    with pytest.raises(ValueError, match="FO_TEST_CMAKE_TIMEOUT must be a positive number of seconds"):
        _timeouts.read_cmake_timeout({_timeouts.CMAKE_TIMEOUT_ENV: value})
