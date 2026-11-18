//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef GAMESTATS_H
#define GAMESTATS_H
#ifdef _WIN32
#pragma once
#endif

enum LogEvent_t
{
	LOGEVENT_UNK0 = 0,
	LOGEVENT_UNK1,
	LOGEVENT_UNK2,
	LOGEVENT_UNK3,
	LOGEVENT_UNK4,
	LOGEVENT_UNK5,
	LOGEVENT_COMMENTARY,
	LOGEVENT_CREDITS,
	LOGEVENT_UNK8,
	LOGEVENT_UNK9,
	LOGEVENT_UNK10,
	LOGEVENT_UNK11,
	LOGEVENT_UNK12,
	LOGEVENT_UNK13,
	LOGEVENT_UNK14,
	LOGEVENT_UNK15,
	LOGEVENT_UNK16,
	LOGEVENT_PLAYER_KILLED,
	LOGEVENT_UNK18,
	LOGEVENT_UNK19,
	LOGEVENT_UNK20,
};

abstract_class IGameStats
{
public:
	virtual void LogEvent( bool unk0, LogEvent_t unk1, const char* unk2 ) = 0;
};

extern IGameStats *gamestats;

#endif // GAMESTATS_H
