"""Native safety audits for Engine and project C++ sources.

``sync_scope`` enforces the native entity-cover boundary of server script
exports; ``exception_safety`` gates per-function exception-safety
classifications against committed baselines. ``BuildTools/native_safety_audit.py``
is the command line entry point.
"""
