//========= Copyright (c) 2026, improved-srcds-v34 contributors ============//
//
// Purpose: Server-side validation of client-uploaded sprays and gating of the
//          "Player Decal" broadcast that makes clients decode them.
//
//=============================================================================//

#ifndef SPRAYCHECK_H
#define SPRAYCHECK_H
#ifdef _WIN32
#pragma once
#endif

class IGameSystem;

// True when a spray decal must not be broadcast: sprays are disabled, the
// validator is still scanning, or it found a spray it had to quarantine.
bool SprayCheck_BlockDecals( void );

// Queues (or restarts) a scan of the mod's downloads/ tree. Cheap; the work is
// spread over frames by the game system.
void SprayCheck_RequestScan( void );

// Game system that services scan work within a per-frame budget.
IGameSystem *SprayCheckSystem( void );

#endif // SPRAYCHECK_H
