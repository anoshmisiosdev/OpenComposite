# ci/compat — case-sensitivity shims

App and OpenXR-SDK sources include Windows headers with mixed case
(`<Windows.h>`, `<PathCch.h>`, `<SDKDDKVer.h>`, `<ShlObj.h>`, `<DirectXMath.h>`).
mingw-w64 ships these lowercase, so the includes resolve on **case-insensitive**
macOS but fail on the **case-sensitive** Linux CI runner.

Each file here forwards the capitalized name to the lowercase real header. The CI
build adds `-I ci/compat` (Linux only). The local macOS build must NOT add it —
on a case-insensitive FS the shim would resolve `<pathcch.h>` back to itself.
