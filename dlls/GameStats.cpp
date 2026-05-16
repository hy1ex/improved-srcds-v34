//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "GameStats.h"

class CGameStats : public IGameStats
{
public:
	void LogEvent( bool unk0, LogEvent_t unk1, const char* unk2 )
	{
	}
};

static CGameStats g_GameStatsSingleton;

IGameStats *gamestats = &g_GameStatsSingleton;
