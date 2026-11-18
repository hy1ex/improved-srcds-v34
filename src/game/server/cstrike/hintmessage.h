//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef HINTMESSAGE_H
#define HINTMESSAGE_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"
#include "simtimer.h"

#ifdef GAME_DLL
	#include "player.h"
#else
	#include "c_baseplayer.h"
#endif

class CHintSystem;

//--------------------------------------------------------------------------------------------------------------
class CHintMessage
{
public:
	CHintMessage( const char * hintString, CUtlVector< const char * > * args, float duration );
	~CHintMessage();

	float GetDuration() const { return m_duration; }
	void Send( CBasePlayer *client );

	bool IsEquivalent( const char *hintString, CUtlVector< const char * > * args ) const;

private:
	const char * m_hintString;					///< hintString is a pointer to a string that should never be deleted.
	CUtlVector< char * > m_args;				///< list of arguments.  The memory for these strings is internal to the CHintMessage.
	float m_duration;							///< time until the next message can be displayed
};


//--------------------------------------------------------------------------------------------------------------
class CHintMessageQueue
{
public:
	CHintMessageQueue( CBasePlayer *pPlayer );
	void		Reset();
	void		Update();
	bool		AddMessage( const char* message, float duration = 6.0f, CUtlVector< const char * > * args = NULL );
	inline bool IsEmpty() { return m_messages.Count() == 0; }

private:
	float		m_tmMessageEnd;
	CUtlVector< CHintMessage * > m_messages;
	CBasePlayer *m_pPlayer;
};

#endif // HINTMESSAGE_H
