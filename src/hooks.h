#pragma once

#include <windows.h>

// Installs the CTxdStore and CTimer::Update hooks through MinHook. Runs under
// the loader lock, because the hooks must exist before the game loads its
// first texture dictionary; everything else waits for StartInitialisation.
bool InstallHooks(HMODULE module);
void MarkHooksInstalled();

// Creates and scans the textures folder.
// Runs once, on the first hook call or on the startup thread.
void StartInitialisation();
