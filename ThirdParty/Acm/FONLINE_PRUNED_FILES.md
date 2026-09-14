FOnline ThirdParty pruning notes

This vendored copy is intentionally limited to the files used by the engine.
When updating from upstream, keep it minimal again before committing.

Removed paths:
- None. This directory already contains only the ACM stream source and header.

Local patches (FOnline Patch):
- acmstrm.h, acmstrm.cpp: the amplitude dictionary `Amplitude_Buffer` / `Buffer_Middle` is a
  CACMUnpacker member instead of a file-level global, so two streams decoded at once do not
  overwrite each other's dictionary.

