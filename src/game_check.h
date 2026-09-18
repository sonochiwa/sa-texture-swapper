#pragma once

// True when the process is GTA San Andreas 1.0 US: identified by the PE
// headers, which no plugin rewrites, or failing that by the prologues of the
// functions the plugin hooks, tolerating ones another plugin has already
// detoured. On false, `what` names the mismatch. Only memory comparisons
// happen here, so it is safe under the loader lock.
bool CheckGameVersion(const char** what);
