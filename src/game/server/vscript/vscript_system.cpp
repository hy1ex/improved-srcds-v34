//========= Copyright (c) 2026, improved-srcds-v34 contributors ============//
//
// Purpose: Lifecycle and glue between the game DLL and the VScript VM:
//          map-shipped `vscripts` keyvalues, per-entity scopes, think
//          functions, deferred EntFire, gameplay hooks and game-event
//          listeners.
//
//          Everything here is inert while sv_vscript_enable is 0: the hooks
//          bail out on the first check and the game system does nothing.
//
//=============================================================================//

#include "cbase.h"
#include "vscript.h"
#include "vscript_system.h"
#include "igamesystem.h"
#include "entitylist.h"
#include "eventqueue.h"
#include "igameevents.h"
#include "filesystem.h"
#include "utlvector.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar sv_vscript_enable;
extern ConVar sv_vscript_debug;

//-----------------------------------------------------------------------------
// Per-entity script state. The entry is keyed by the entity pointer but the
// handle is the authority: when the entity is destroyed the handle goes NULL
// and the entry is dropped, so a recycled allocation cannot inherit scripts.
//-----------------------------------------------------------------------------
struct ScriptedEntity_t
{
	CHandle<CBaseEntity>	hEntity;
	CUtlString				strScripts;
	HSCRIPT					hScope;
	bool					bLoaded;
};

struct ScriptThink_t
{
	CHandle<CBaseEntity>	hEntity;
	CUtlString				strFunc;
	float					flNextThink;
};

struct PendingEntFire_t
{
	CHandle<CBaseEntity>	hTarget;
	CUtlString				strTargetName;
	CUtlString				strInput;
	CUtlString				strValue;
	CHandle<CBaseEntity>	hActivator;
	CHandle<CBaseEntity>	hCaller;
};

struct EventListener_t
{
	int			iId;
	CUtlString	strEvent;
	CUtlString	strFunc;
	HSCRIPT		hScope;
};

struct EventFanout_t
{
	CUtlString			strEvent;
	IGameEventListener2	*pListener;
};

namespace
{
	CUtlVector<ScriptedEntity_t>	g_ScriptedEntities;
	CUtlVector<ScriptThink_t>		g_Thinks;
	CUtlVector<PendingEntFire_t>	g_PendingEntFire;
	CUtlVector<EventListener_t>		g_EventListeners;
	CUtlVector<EventFanout_t>		g_EventFanouts;
	int								g_iNextListenerId = 1;
	bool							g_bMapCallbacksRegistered = false;

	ScriptThink_t *FindThink( CBaseEntity *pEntity, const char *pszFunc )
	{
		for ( int i = 0; i < g_Thinks.Count(); i++ )
		{
			ScriptThink_t &think = g_Thinks[i];
			if ( think.hEntity.Get() == pEntity && !Q_stricmp( think.strFunc.Get(), pszFunc ) )
				return &think;
		}
		return NULL;
	}

	ScriptedEntity_t *FindScriptedEntity( CBaseEntity *pEntity )
	{
		for ( int i = 0; i < g_ScriptedEntities.Count(); i++ )
		{
			if ( g_ScriptedEntities[i].hEntity.Get() == pEntity )
				return &g_ScriptedEntities[i];
		}
		return NULL;
	}

	void RemoveScriptedEntityEntry( CBaseEntity *pEntity )
	{
		for ( int i = g_ScriptedEntities.Count() - 1; i >= 0; i-- )
		{
			if ( g_ScriptedEntities[i].hEntity.Get() == pEntity )
				g_ScriptedEntities.Remove( i );
		}
	}

	void RemoveEntityListeners( HSCRIPT hScope )
	{
		if ( !hScope )
			return;

		for ( int i = g_EventListeners.Count() - 1; i >= 0; i-- )
		{
			if ( g_EventListeners[i].hScope == hScope )
				g_EventListeners.Remove( i );
		}
	}

	void ClearAllState()
	{
		for ( int i = 0; i < g_ScriptedEntities.Count(); i++ )
		{
			if ( g_ScriptedEntities[i].hScope )
				VScript_DestroyScope( g_ScriptedEntities[i].hScope );
		}
		g_ScriptedEntities.RemoveAll();
		g_Thinks.RemoveAll();
		g_PendingEntFire.RemoveAll();
		g_EventListeners.RemoveAll();

		for ( int i = 0; i < g_EventFanouts.Count(); i++ )
		{
			if ( g_EventFanouts[i].pListener && gameeventmanager )
				gameeventmanager->RemoveListener( g_EventFanouts[i].pListener );
			delete g_EventFanouts[i].pListener;
		}
		g_EventFanouts.RemoveAll();
		g_bMapCallbacksRegistered = false;
	}
}

//-----------------------------------------------------------------------------
// Game event payload adapter. The v34 IGameEvent interface has no key
// enumeration, so the adapter probes the keys CS:S events actually carry.
// Scripts read event.<key>; absent keys are simply not present in the table.
//-----------------------------------------------------------------------------
namespace
{
	const char *s_pEventKeys[] =
	{
		"userid", "victim", "attacker", "assister", "weapon", "headshot",
		"dmg_health", "health", "team", "priority", "objective", "reason",
		"winner", "message", "name", "index", "hitgroup", "damage", "distance",
		"speed", "pwned", "penetrated", "text", "maxtime", "time", "roundslimit",
		"fraglimit", "timelimit", "placeholder", "oldteam", "newteam", "isbot",
		"bot", "site", "bomb", "entindex"
	};

	bool IsIntEventKey( const char *pszKey )
	{
		static const char *s_pKeys[] =
		{
			"userid", "victim", "attacker", "assister", "health", "dmg_health",
			"team", "priority", "index", "hitgroup", "oldteam", "newteam", "isbot",
			"bot", "entindex", "pwned", "penetrated", "headshot", "winner", "reason"
		};

		for ( int i = 0; i < ARRAYSIZE( s_pKeys ); i++ )
		{
			if ( !Q_stricmp( pszKey, s_pKeys[i] ) )
				return true;
		}
		return false;
	}

	bool IsFloatEventKey( const char *pszKey )
	{
		static const char *s_pKeys[] =
		{
			"distance", "speed", "time", "maxtime", "roundslimit", "fraglimit",
			"timelimit", "damage"
		};

		for ( int i = 0; i < ARRAYSIZE( s_pKeys ); i++ )
		{
			if ( !Q_stricmp( pszKey, s_pKeys[i] ) )
				return true;
		}
		return false;
	}

	class CScriptGameEvent : public IScriptEventAccess
	{
	public:
		CScriptGameEvent( IGameEvent *pEvent ) : m_pEvent( pEvent ) {}

		virtual const char *Name() const
		{
			return m_pEvent ? m_pEvent->GetName() : "";
		}

		virtual int Count() const
		{
			return ARRAYSIZE( s_pEventKeys );
		}

		virtual const char *Key( int i ) const
		{
			if ( i < 0 || i >= ARRAYSIZE( s_pEventKeys ) )
				return NULL;
			return s_pEventKeys[i];
		}

		virtual int Type( int i ) const
		{
			const char *pszKey = Key( i );
			if ( !m_pEvent || !pszKey || m_pEvent->IsEmpty( pszKey ) )
				return SCRIPT_VOID;

			if ( IsIntEventKey( pszKey ) )
				return SCRIPT_INT;
			if ( IsFloatEventKey( pszKey ) )
				return SCRIPT_FLOAT;
			return SCRIPT_STRING;
		}

		virtual int Int( int i ) const { return m_pEvent ? m_pEvent->GetInt( Key( i ) ) : 0; }
		virtual float Float( int i ) const { return m_pEvent ? m_pEvent->GetFloat( Key( i ) ) : 0.0f; }
		virtual bool Bool( int i ) const { return m_pEvent ? m_pEvent->GetBool( Key( i ) ) : false; }
		virtual const char *String( int i ) const { return m_pEvent ? m_pEvent->GetString( Key( i ) ) : ""; }

	private:
		IGameEvent *m_pEvent;
	};

	class CScriptEventFanout : public IGameEventListener2
	{
	public:
		virtual void FireGameEvent( IGameEvent *event )
		{
			if ( !event || !VScript_IsActive() )
				return;

			const char *pszName = event->GetName();
			if ( !pszName )
				return;

			CScriptGameEvent payload( event );

			// Copy the matching entries first: callbacks may add or remove
			// listeners while we are iterating in the vector.
			CUtlVector<EventListener_t> callbacks;
			for ( int i = 0; i < g_EventListeners.Count(); i++ )
			{
				if ( !Q_stricmp( g_EventListeners[i].strEvent.Get(), pszName ) )
					callbacks.AddToTail( g_EventListeners[i] );
			}

			for ( int i = 0; i < callbacks.Count(); i++ )
			{
				VScript_CallEventCallback( callbacks[i].hScope, callbacks[i].strFunc.Get(), payload );
			}

			// Documented hook names on the global scope and the map scope.
			char szHook[128];
			Q_snprintf( szHook, sizeof( szHook ), "OnGameEvent_%s", pszName );
			VScript_CallEventCallback( 0, szHook, payload );
		}
	};
}

//-----------------------------------------------------------------------------
// Game event registration
//-----------------------------------------------------------------------------
void VScript_OnCollectGameEventCallback( const char *pszEventName, HSCRIPT hScope )
{
	if ( !pszEventName || !pszEventName[0] )
		return;

	char szFunc[128];
	Q_snprintf( szFunc, sizeof( szFunc ), "OnGameEvent_%s", pszEventName );

	VScript_ListenToGameEvent( pszEventName, hScope, szFunc );
}

int VScript_ListenToGameEvent( const char *pszEvent, HSCRIPT hScope, const char *pszFunc )
{
	if ( !VScript_IsActive() || !pszEvent || !pszEvent[0] || !pszFunc || !pszFunc[0] )
		return 0;

	// One listener object per event name, created lazily and kept for the map.
	bool bHaveFanout = false;
	for ( int i = 0; i < g_EventFanouts.Count(); i++ )
	{
		if ( !Q_stricmp( g_EventFanouts[i].strEvent.Get(), pszEvent ) )
		{
			bHaveFanout = true;
			break;
		}
	}

	if ( !bHaveFanout )
	{
		CScriptEventFanout *pFanout = new CScriptEventFanout();
		if ( !gameeventmanager || !gameeventmanager->AddListener( pFanout, pszEvent, true ) )
		{
			VScript_Warn( "ListenToGameEvent: unknown or unavailable event '%s'", pszEvent );
			delete pFanout;
			return 0;
		}

		EventFanout_t fanout;
		fanout.strEvent = pszEvent;
		fanout.pListener = pFanout;
		g_EventFanouts.AddToTail( fanout );
	}

	EventListener_t listener;
	listener.iId = g_iNextListenerId++;
	listener.strEvent = pszEvent;
	listener.strFunc = pszFunc;
	listener.hScope = hScope;
	g_EventListeners.AddToTail( listener );

	return listener.iId;
}

void VScript_StopListeningToGameEvent( int hListener )
{
	for ( int i = g_EventListeners.Count() - 1; i >= 0; i-- )
	{
		if ( g_EventListeners[i].iId == hListener )
			g_EventListeners.Remove( i );
	}
}

//-----------------------------------------------------------------------------
// Entity scripts
//-----------------------------------------------------------------------------
bool VScript_HandleEntityKeyValue( CBaseEntity *pEntity, const char *pszKey, const char *pszValue )
{
	if ( !VScript_IsActive() || !pEntity || !pszKey )
		return false;

	if ( Q_stricmp( pszKey, "vscripts" ) )
		return false;

	if ( !pszValue || !pszValue[0] )
		return true;

	ScriptedEntity_t *pEntry = FindScriptedEntity( pEntity );
	if ( !pEntry )
	{
		ScriptedEntity_t entry;
		entry.hEntity = pEntity;
		entry.hScope = 0;
		entry.bLoaded = false;
		g_ScriptedEntities.AddToTail( entry );
		pEntry = &g_ScriptedEntities[g_ScriptedEntities.Count() - 1];
	}

	pEntry->strScripts = pszValue;
	pEntry->bLoaded = false;
	return true;
}

static void LoadEntityScripts( ScriptedEntity_t *pEntry )
{
	CBaseEntity *pEntity = pEntry->hEntity.Get();
	if ( !pEntity )
		return;

	if ( !pEntry->hScope )
		pEntry->hScope = VScript_CreateScope( pEntity );

	// Space separated file list, exactly as Valve parses the keyvalue.
	char szList[512];
	Q_strncpy( szList, pEntry->strScripts.Get(), sizeof( szList ) );

	char *p = szList;
	while ( p && *p )
	{
		while ( *p == ' ' || *p == '\t' )
			p++;

		if ( !*p )
			break;

		char *pStart = p;
		while ( *p && *p != ' ' && *p != '\t' )
			p++;

		char szFile[MAX_PATH];
		int nLen = p - pStart;
		if ( nLen <= 0 || nLen >= (int)sizeof( szFile ) )
			continue;

		Q_strncpy( szFile, pStart, nLen + 1 );
		VScript_RunFileInScope( szFile, pEntry->hScope, true );
	}

	pEntry->bLoaded = true;

	if ( VScript_HasScopeFunction( pEntry->hScope, "Precache" ) )
		VScript_CallScopeFunction( pEntry->hScope, "Precache", pEntity, 0, NULL, false );

	if ( VScript_HasScopeFunction( pEntry->hScope, "OnPostSpawn" ) )
		VScript_CallScopeFunction( pEntry->hScope, "OnPostSpawn", pEntity, 0, NULL, false );

	if ( VScript_HasScopeFunction( pEntry->hScope, "ConnectOutputs" ) )
		VScript_CallScopeFunction( pEntry->hScope, "ConnectOutputs", pEntity, 0, NULL, false );

	VScript_CollectGameEventCallbacks( pEntry->hScope );
}

void VScript_EntitySpawned( CBaseEntity *pEntity )
{
	if ( !VScript_IsActive() || !pEntity )
		return;

	ScriptedEntity_t *pEntry = FindScriptedEntity( pEntity );
	if ( !pEntry || pEntry->bLoaded )
		return;

	LoadEntityScripts( pEntry );
}

void VScript_EntityRemoved( CBaseEntity *pEntity )
{
	if ( !pEntity )
		return;

	ScriptedEntity_t *pEntry = FindScriptedEntity( pEntity );
	if ( pEntry )
	{
		if ( pEntry->hScope )
		{
			RemoveEntityListeners( pEntry->hScope );
			VScript_DestroyScope( pEntry->hScope );
		}
		RemoveScriptedEntityEntry( pEntity );
	}

	for ( int i = g_Thinks.Count() - 1; i >= 0; i-- )
	{
		if ( g_Thinks[i].hEntity.Get() == NULL || g_Thinks[i].hEntity.Get() == pEntity )
			g_Thinks.Remove( i );
	}
}

void VScript_AddThinkToEntity( CBaseEntity *pEntity, const char *pszFunc, float flDelay )
{
	if ( !VScript_IsActive() || !pEntity || !pszFunc || !pszFunc[0] )
		return;

	ScriptThink_t *pThink = FindThink( pEntity, pszFunc );
	if ( pThink )
	{
		pThink->flNextThink = gpGlobals->curtime + flDelay;
		return;
	}

	ScriptThink_t think;
	think.hEntity = pEntity;
	think.strFunc = pszFunc;
	think.flNextThink = gpGlobals->curtime + flDelay;
	g_Thinks.AddToTail( think );
}

//-----------------------------------------------------------------------------
// Entity input queue
//-----------------------------------------------------------------------------
void VScript_QueueEntFire( CBaseEntity *pTarget, const char *pszTargetName, const char *pszInput,
						   const char *pszValue, float flDelay, CBaseEntity *pActivator, CBaseEntity *pCaller )
{
	if ( !VScript_IsActive() )
		return;

	if ( ( !pTarget && ( !pszTargetName || !pszTargetName[0] ) ) || !pszInput || !pszInput[0] )
		return;

	variant_t value;
	value.SetString( AllocPooledString( pszValue ? pszValue : "" ) );

	// Delayed inputs go through the engine's event queue exactly like map
	// logic does; zero-delay inputs are deferred to the post-think flush so a
	// script cannot reenter its own think/input chain.
	if ( flDelay > 0.0f )
	{
		if ( pTarget )
			g_EventQueue.AddEvent( pTarget, pszInput, value, flDelay, pActivator, pCaller );
		else
			g_EventQueue.AddEvent( pszTargetName, pszInput, value, flDelay, pActivator, pCaller );
		return;
	}

	PendingEntFire_t pending;
	pending.hTarget = pTarget;
	pending.strTargetName = pszTargetName ? pszTargetName : "";
	pending.strInput = pszInput;
	pending.strValue = pszValue ? pszValue : "";
	pending.hActivator = pActivator;
	pending.hCaller = pCaller;
	g_PendingEntFire.AddToTail( pending );
}

static void FlushEntFireQueue()
{
	if ( g_PendingEntFire.Count() == 0 )
		return;

	CUtlVector<PendingEntFire_t> pending;
	pending.AddVectorToTail( g_PendingEntFire );
	g_PendingEntFire.RemoveAll();

	for ( int i = 0; i < pending.Count(); i++ )
	{
		PendingEntFire_t &entry = pending[i];

		CBaseEntity *pTarget = entry.hTarget.Get();
		if ( !pTarget )
		{
			if ( entry.strTargetName.Get()[0] )
				pTarget = gEntList.FindEntityByName( NULL, entry.strTargetName.Get() );
		}

		if ( !pTarget )
			continue;

		variant_t value;
		value.SetString( AllocPooledString( entry.strValue.Get() ) );
		pTarget->AcceptInput( entry.strInput.Get(), entry.hActivator.Get(), entry.hCaller.Get(), value, 0 );
	}
}

//-----------------------------------------------------------------------------
// Script thinks
//-----------------------------------------------------------------------------
static void RunScriptThinks()
{
	if ( g_Thinks.Count() == 0 )
		return;

	CUtlVector<ScriptThink_t> thinks;
	thinks.AddVectorToTail( g_Thinks );

	for ( int i = 0; i < thinks.Count(); i++ )
	{
		ScriptThink_t &think = thinks[i];
		if ( think.flNextThink > gpGlobals->curtime )
			continue;

		CBaseEntity *pEntity = think.hEntity.Get();
		if ( !pEntity )
			continue;

		HSCRIPT hScope = VScript_GetOwnerScope( pEntity );
		if ( !hScope )
			continue;

		if ( !VScript_HasScopeFunction( hScope, think.strFunc.Get() ) )
			continue;

		float flNext = 0.0f;
		VScript_CallScopeFunction( hScope, think.strFunc.Get(), pEntity, 0, NULL, false, &flNext );

		ScriptThink_t *pLive = FindThink( pEntity, think.strFunc.Get() );
		if ( pLive )
			pLive->flNextThink = gpGlobals->curtime + ( flNext > 0.0f ? flNext : 0.0f );
	}
}

//-----------------------------------------------------------------------------
// Gameplay hooks
//-----------------------------------------------------------------------------
void VScript_OnPlayerSpawn( CBasePlayer *pPlayer )
{
	if ( !VScript_IsActive() || !pPlayer )
		return;

	ScriptArg_t arg = ScriptArg_Entity( pPlayer );
	VScript_CallScopeFunction( 0, "OnPlayerSpawn", pPlayer, 1, &arg, false );
}

void VScript_OnPlayerKilled( CBasePlayer *pVictim, CBaseEntity *pAttacker, const char *pszWeapon )
{
	if ( !VScript_IsActive() || !pVictim )
		return;

	ScriptArg_t args[3];
	args[0] = ScriptArg_Entity( pVictim );
	args[1] = ScriptArg_Entity( pAttacker );
	args[2] = ScriptArg_String( pszWeapon ? pszWeapon : "" );

	VScript_CallScopeFunction( 0, "OnPlayerKilled", pVictim, 3, args, false );
}

void VScript_OnRoundStart()
{
	if ( !VScript_IsActive() )
		return;

	VScript_CallScopeFunction( 0, "OnRoundStart", NULL, 0, NULL, false );
}

void VScript_OnRoundEnd()
{
	if ( !VScript_IsActive() )
		return;

	VScript_CallScopeFunction( 0, "OnRoundEnd", NULL, 0, NULL, false );
}

//-----------------------------------------------------------------------------
// Argument constructors
//-----------------------------------------------------------------------------
ScriptArg_t ScriptArg_Int( int nValue )
{
	ScriptArg_t arg;
	arg.iType = SCRIPT_INT;
	arg.iValue = nValue;
	return arg;
}

ScriptArg_t ScriptArg_Float( float flValue )
{
	ScriptArg_t arg;
	arg.iType = SCRIPT_FLOAT;
	arg.flValue = flValue;
	return arg;
}

ScriptArg_t ScriptArg_Bool( bool bValue )
{
	ScriptArg_t arg;
	arg.iType = SCRIPT_BOOL;
	arg.bValue = bValue;
	return arg;
}

ScriptArg_t ScriptArg_String( const char *pszValue )
{
	ScriptArg_t arg;
	arg.iType = SCRIPT_STRING;
	arg.strValue = pszValue ? pszValue : "";
	return arg;
}

ScriptArg_t ScriptArg_Vector( const Vector &vecValue )
{
	ScriptArg_t arg;
	arg.iType = SCRIPT_VECTOR;
	arg.vecValue = vecValue;
	return arg;
}

ScriptArg_t ScriptArg_Entity( CBaseEntity *pEntity )
{
	ScriptArg_t arg;
	arg.iType = SCRIPT_ENTITY;
	arg.hValue = 0;
	arg.iValue = (int)(long)pEntity;
	return arg;
}

//-----------------------------------------------------------------------------
// Level lifecycle
//-----------------------------------------------------------------------------
bool VScript_LevelInitPostEntity()
{
	if ( !VScript_IsActive() )
		return false;

	// Server-operator hook file, optional.
	VScript_RunFileInScope( "mapspawn.nut", 0, false );

	if ( VScript_HasScopeFunction( 0, "OnMapSpawn" ) )
		VScript_CallScopeFunction( 0, "OnMapSpawn", NULL, 0, NULL, false );

	if ( !g_bMapCallbacksRegistered )
	{
		VScript_ListenToGameEvent( "round_start", 0, "OnRoundStart" );
		VScript_ListenToGameEvent( "round_end", 0, "OnRoundEnd" );
		VScript_CollectGameEventCallbacks( 0 );
		g_bMapCallbacksRegistered = true;
	}

	return true;
}

void VScript_LevelShutdown()
{
	ClearAllState();
	VScript_Shutdown();
}

//-----------------------------------------------------------------------------
// Console commands
//-----------------------------------------------------------------------------
static void EnsureScripting()
{
	if ( VScript_IsActive() )
		return;

	if ( !sv_vscript_enable.GetBool() )
	{
		Msg( "VScript is disabled: set sv_vscript_enable 1 first\n" );
		return;
	}

	VScript_Init();
}

static void CC_script_execute( void )
{
	EnsureScripting();
	if ( !VScript_IsActive() )
		return;

	if ( engine->Cmd_Argc() < 2 )
	{
		Msg( "usage: script_execute <file>\n" );
		return;
	}

	const char *pszFile = engine->Cmd_Argv( 1 );
	if ( !VScript_RunFileInScope( pszFile, 0, true ) )
		Msg( "script_execute: failed to run %s\n", pszFile );
}

static void CC_script_reload( void )
{
	if ( !VScript_IsActive() )
	{
		Msg( "script_reload: VScript is not running\n" );
		return;
	}

	for ( int i = 0; i < g_ScriptedEntities.Count(); i++ )
	{
		ScriptedEntity_t &entry = g_ScriptedEntities[i];
		if ( entry.hScope )
		{
			RemoveEntityListeners( entry.hScope );
			VScript_DestroyScope( entry.hScope );
			entry.hScope = 0;
		}
		entry.bLoaded = false;
	}

	g_Thinks.RemoveAll();
	g_PendingEntFire.RemoveAll();
	g_EventListeners.RemoveAll();

	for ( int i = 0; i < g_ScriptedEntities.Count(); i++ )
		LoadEntityScripts( &g_ScriptedEntities[i] );

	VScript_RunFileInScope( "mapspawn.nut", 0, false );
	Msg( "script_reload: %d entity script set(s) reloaded\n", g_ScriptedEntities.Count() );
}

static void CC_script_help( void )
{
	const char *pszFilter = ( engine->Cmd_Argc() > 1 ) ? engine->Cmd_Argv( 1 ) : NULL;

	Msg( "VScript bindings:\n" );
	for ( int i = 0; i < g_nScriptGlobalBindings; i++ )
	{
		if ( pszFilter && !V_stristr( g_ScriptGlobalBindings[i].pszName, pszFilter ) )
			continue;
		Msg( "  %-28s %s\n", g_ScriptGlobalBindings[i].pszName, g_ScriptGlobalBindings[i].pszHelp );
	}

	Msg( "CBaseEntity methods:\n" );
	for ( int i = 0; i < g_nScriptEntityBindings; i++ )
	{
		if ( pszFilter && !V_stristr( g_ScriptEntityBindings[i].pszName, pszFilter ) )
			continue;
		Msg( "  %-28s %s\n", g_ScriptEntityBindings[i].pszName, g_ScriptEntityBindings[i].pszHelp );
	}
}

static ConCommand script_execute( "script_execute", CC_script_execute,
	"Run a .nut file from scripts/vscripts in the global scope", FCVAR_CHEAT );
static ConCommand script_reload( "script_reload", CC_script_reload,
	"Reload all map-shipped VScript files", FCVAR_CHEAT );
static ConCommand script_help( "script_help", CC_script_help,
	"List the VScript API (optional name filter)" );

//-----------------------------------------------------------------------------
// Game system
//-----------------------------------------------------------------------------
class CVScriptGameSystem : public CBaseGameSystemPerFrame
{
public:
	virtual char const *Name() { return "CVScriptGameSystem"; }

	virtual bool Init()
	{
		return true;
	}

	virtual void LevelInitPreEntity()
	{
		// The VM has to exist before map entities are parsed: their `vscripts`
		// keyvalues and spawn-time callbacks depend on it.
		if ( sv_vscript_enable.GetBool() )
			VScript_Init();
	}

	virtual void LevelInitPostEntity()
	{
		VScript_LevelInitPostEntity();
	}

	virtual void LevelShutdownPreEntity()
	{
		VScript_LevelShutdown();
	}

	virtual void FrameUpdatePreEntityThink()
	{
		if ( !VScript_IsActive() )
			return;

		VScript_Frame();
		RunScriptThinks();
	}

	virtual void FrameUpdatePostEntityThink()
	{
		if ( !VScript_IsActive() )
			return;

		FlushEntFireQueue();

		// Drop bookkeeping for entities that no longer exist.
		for ( int i = g_ScriptedEntities.Count() - 1; i >= 0; i-- )
		{
			if ( g_ScriptedEntities[i].hEntity.Get() == NULL )
				g_ScriptedEntities.Remove( i );
		}

		for ( int i = g_Thinks.Count() - 1; i >= 0; i-- )
		{
			if ( g_Thinks[i].hEntity.Get() == NULL )
				g_Thinks.Remove( i );
		}
	}
};

static CVScriptGameSystem g_VScriptGameSystem;

IGameSystem *VScriptSystem()
{
	return &g_VScriptGameSystem;
}
