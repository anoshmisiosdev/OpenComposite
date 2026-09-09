// Case-insensitive-FS compat shim (see ci/compat/README). Forwards to the
// lowercase header mingw-w64 provides; used only on the case-sensitive CI runner.
#include <windows.h>
