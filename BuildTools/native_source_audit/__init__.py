"""Native source audits that enforce the Engine coding contracts on C++ sources.

The command-line entry point is ``BuildTools/native_source_audit.py``.  Each audit module scans a set of
root-relative scopes, applies the Engine-owned baseline plus optional project baselines and reports the
remaining findings.
"""
