"""Shared subprocess timeouts for tests that run CMake configure, build and script steps."""

from __future__ import annotations

import math
import os

CMAKE_TIMEOUT_ENV = "FO_TEST_CMAKE_TIMEOUT"
DEFAULT_CMAKE_TIMEOUT_SECONDS = 600.0


def read_cmake_timeout(environment: dict[str, str] | os._Environ[str] = os.environ) -> float:
    """Return the CMake step timeout in seconds; an invalid override fails instead of falling back."""

    raw = environment.get(CMAKE_TIMEOUT_ENV)

    if raw is None or raw.strip() == "":
        return DEFAULT_CMAKE_TIMEOUT_SECONDS

    try:
        value = float(raw)
    except ValueError as error:
        raise ValueError(f"{CMAKE_TIMEOUT_ENV} must be a positive number of seconds, got {raw!r}") from error

    if not math.isfinite(value) or value <= 0:
        raise ValueError(f"{CMAKE_TIMEOUT_ENV} must be a positive number of seconds, got {raw!r}")

    return value


# A configure or build of a tiny fixture project normally takes seconds, but a loaded host stretches it far past any
# tight limit, so one generous shared value replaces the per-file constants
CMAKE_TIMEOUT_SECONDS = read_cmake_timeout()
