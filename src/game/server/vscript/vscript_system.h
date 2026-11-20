//========= Copyright (c) 2026, improved-srcds-v34 contributors ============//
//
// Purpose: Registers the VScript game system with the server DLL.
//
//=============================================================================//

#ifndef VSCRIPT_SYSTEM_H
#define VSCRIPT_SYSTEM_H
#ifdef GAME_DLL

class IGameSystem;

IGameSystem *VScriptSystem();

#endif // GAME_DLL
#endif // VSCRIPT_SYSTEM_H
