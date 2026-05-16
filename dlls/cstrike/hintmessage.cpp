//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "hintmessage.h"

#ifdef GAME_DLL
	#include "util.h"
#endif

//--------------------------------------------------------------------------------------------------------
/**
* Simple utility function to allocate memory and duplicate a string
*/
inline char *CloneString( const char *str )
{
	char *cloneStr = new char [ strlen(str)+1 ];
	strcpy( cloneStr, str );
	return cloneStr;
}

extern int gmsgHudText;

enum { HMQ_SIZE = 8 };	// Maximum number of messages queue can hold
						// If the limit is reached, no more can be
						// added.

//--------------------------------------------------------------------------------------------------------------
CHintMessage::CHintMessage( const char * hintString, CUtlVector< const char * > * args, float duration )
{
	m_hintString = hintString;
	m_duration = duration;

	if ( args )
	{
		for ( int i=0; i<args->Count(); ++i )
		{
			m_args.AddToTail( CloneString( (*args)[i] ) );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
CHintMessage::~CHintMessage()
{
	for ( int i=0; i<m_args.Count(); ++i )
	{
		delete[] m_args[i];
	}
	m_args.RemoveAll();
}

//--------------------------------------------------------------------------------------------------------------
bool CHintMessage::IsEquivalent( const char *hintString, CUtlVector< const char * > * args ) const
{
	if ( FStrEq( hintString, m_hintString ) )
	{
		if ( !args && !m_args.Count() )
		{
			return true;
		}

		if ( !args )
			return false;

		if ( args->Count() != m_args.Count() )
			return false;

		for ( int i=0; i<args->Count(); ++i )
		{
			if ( !FStrEq( (*args)[i], m_args[i] ) )
			{
				return false;
			}
		}
	}
	return false;
}

//--------------------------------------------------------------------------------------------------------------
void CHintMessage::Send( CBasePlayer * client )
{
	if ( !client )
		return;

#ifdef GAME_DLL
	// Custom hint text sending to allow for arguments.  This is OK because the client has a custom
	// message parser for hint text that can read the arguments.
	CSingleUserRecipientFilter user( (CBasePlayer *)client );
	user.MakeReliable();

	// client can handle 1 string only
	UserMessageBegin( user, "HintText" );
	WRITE_BYTE( m_args.Count() + 1 );
	WRITE_STRING( m_hintString );
	for ( int i = 0; i < m_args.Count(); ++i )
		WRITE_STRING( m_args[i] );
	MessageEnd();
#endif
}

//--------------------------------------------------------------------------------------------------------------
CHintMessageQueue::CHintMessageQueue( CBasePlayer *pPlayer )
{
	m_pPlayer = pPlayer;
}

//--------------------------------------------------------------------------------------------------------------
void CHintMessageQueue::Reset()
{
	m_tmMessageEnd = 0;
	for ( int i=0; i<m_messages.Count(); ++i )
	{
		delete m_messages[i];
	}
	m_messages.RemoveAll();
}

//--------------------------------------------------------------------------------------------------------------
void CHintMessageQueue::Update()
{
	if ( !m_pPlayer )
		return;

	// test this - send the message as soon as it is ready, 
	// just stomp the old message
	if ( gpGlobals->curtime > m_tmMessageEnd )
	{
		if ( m_messages.Count() )
		{
			CHintMessage *msg = m_messages[0];
			m_tmMessageEnd = gpGlobals->curtime + msg->GetDuration();
			msg->Send( m_pPlayer );
			delete msg;
			m_messages.Remove( 0 );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
bool CHintMessageQueue::AddMessage( const char* message, float duration, CUtlVector< const char * > * args )
{
	if ( !m_pPlayer )
		return false;

	for ( int i=0; i<m_messages.Count(); ++i )
	{
		// weed out duplicates
		if ( m_messages[i]->IsEquivalent( message, args ) )
		{
			return true;
		}
	}

	// 'message' is not copied, so the pointer must remain valid forever
	CHintMessage *msg = new CHintMessage( message, args, duration );
	m_messages.AddToTail( msg );
	return true;
}
