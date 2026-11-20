//========= Copyright (c) 2026, improved-srcds-v34 contributors ============//
//
// Purpose: Server-side VScript, a Squirrel-backed scripting subsystem compiled
//          into the game DLL. The engine (Source 2007, v34) has no scripting
//          interface at all, so the VM lives here and only ever drives entities,
//          convars and game events the stock client already understands.
//
//          This header is the boundary between the VM backend (vscript_vm.cpp,
//          the only file that includes squirrel.h) and the rest of the game DLL.
//          Nothing here changes the network protocol, the entity table or the
//          client.
//
//=============================================================================//

#ifndef VSCRIPT_H
#define VSCRIPT_H
#ifdef GAME_DLL

#include "cbase.h"
#include "utlstring.h"

class CBaseEntity;
class CBasePlayer;

typedef int HSCRIPT;

//-----------------------------------------------------------------------------
// Script value types. Vectors are script objects of the Vector class; entity
// references are instances of CBaseEntity that wrap a handle table entry.
//-----------------------------------------------------------------------------
enum ScriptVariantType_t
{
	SCRIPT_VOID = 0,
	SCRIPT_INT,
	SCRIPT_FLOAT,
	SCRIPT_BOOL,
	SCRIPT_STRING,
	SCRIPT_VECTOR,
	SCRIPT_ENTITY
};

//-----------------------------------------------------------------------------
// A script call: one native binding invocation. Bindings only see this
// interface, so they never touch the VM implementation or its headers.
//-----------------------------------------------------------------------------
class IScriptCallContext
{
public:
	virtual ~IScriptCallContext() {}

	virtual int Count() const = 0;
	virtual bool Exists( int i ) const = 0;
	virtual int ArgType( int i ) const = 0;
	virtual int ArgInt( int i, int nDefault = 0 ) const = 0;
	virtual float ArgFloat( int i, float flDefault = 0.0f ) const = 0;
	virtual bool ArgBool( int i, bool bDefault = false ) const = 0;
	virtual const char *ArgString( int i, const char *pszDefault = "" ) const = 0;
	virtual Vector ArgVector( int i, const Vector &vecDefault = vec3_origin ) const = 0;
	virtual CBaseEntity *ArgEntity( int i ) const = 0;

	// The entity a method was invoked on, or NULL for global functions.
	virtual CBaseEntity *Self() const = 0;

	virtual void ReturnNull() = 0;
	virtual void ReturnInt( int nValue ) = 0;
	virtual void ReturnFloat( float flValue ) = 0;
	virtual void ReturnBool( bool bValue ) = 0;
	virtual void ReturnString( const char *pszValue ) = 0;
	virtual void ReturnVector( const Vector &vecValue ) = 0;
	virtual void ReturnEntity( CBaseEntity *pEntity ) = 0;
	virtual void ReturnScope( HSCRIPT hScope ) = 0;

	// Reports a recoverable script error. The binding keeps running; the error
	// is logged with the binding name and the VM call returns null.
	virtual void Error( const char *pszFormat, ... ) = 0;
};

typedef bool (*ScriptNativeFunc_t)( IScriptCallContext &ctx );

struct ScriptBinding_t
{
	const char *pszName;
	ScriptNativeFunc_t pfn;
	const char *pszHelp;
};

//-----------------------------------------------------------------------------
// A single argument for a C++ -> script call.
//-----------------------------------------------------------------------------
struct ScriptArg_t
{
	int iType;
	int iValue;
	float flValue;
	bool bValue;
	Vector vecValue;
	HSCRIPT hValue;
	CUtlString strValue;

	ScriptArg_t() : iType( SCRIPT_VOID ), iValue( 0 ), flValue( 0.0f ), bValue( false ), vecValue( vec3_origin ), hValue( 0 ) {}
};

void VScript_Warn( const char *pszFormat, ... );

ScriptArg_t ScriptArg_Int( int nValue );
ScriptArg_t ScriptArg_Float( float flValue );
ScriptArg_t ScriptArg_Bool( bool bValue );
ScriptArg_t ScriptArg_String( const char *pszValue );
ScriptArg_t ScriptArg_Vector( const Vector &vecValue );
ScriptArg_t ScriptArg_Entity( CBaseEntity *pEntity );

//-----------------------------------------------------------------------------
// Binding tables, filled in by vscript_bind.cpp.
//-----------------------------------------------------------------------------
extern const ScriptBinding_t g_ScriptGlobalBindings[];
extern const int g_nScriptGlobalBindings;

extern const ScriptBinding_t g_ScriptEntityBindings[];
extern const int g_nScriptEntityBindings;

extern const ScriptBinding_t g_ScriptEntityListBindings[];
extern const int g_nScriptEntityListBindings;

extern const ScriptBinding_t g_ScriptConvarBindings[];
extern const int g_nScriptConvarBindings;

extern const ScriptBinding_t g_ScriptNetPropBindings[];
extern const int g_nScriptNetPropBindings;

extern const ScriptBinding_t g_ScriptVectorBindings[];
extern const int g_nScriptVectorBindings;

//-----------------------------------------------------------------------------
// VM lifecycle, driven by CVScriptGameSystem.
//-----------------------------------------------------------------------------
bool VScript_IsActive();
bool VScript_Init();
void VScript_Shutdown();
void VScript_Frame();

bool VScript_LevelInitPostEntity();
void VScript_LevelShutdown();

//-----------------------------------------------------------------------------
// Scopes, files and calls.
//-----------------------------------------------------------------------------
HSCRIPT VScript_CreateScope( CBaseEntity *pOwner );
void VScript_DestroyScope( HSCRIPT hScope );
HSCRIPT VScript_GetOwnerScope( CBaseEntity *pEntity );
void VScript_SetScopeEntity( HSCRIPT hScope, const char *pszKey, CBaseEntity *pEntity );
void VScript_SetScopeString( HSCRIPT hScope, const char *pszKey, const char *pszValue );
void VScript_SetScopeInt( HSCRIPT hScope, const char *pszKey, int nValue );

bool VScript_RunFileInScope( const char *pszFile, HSCRIPT hScope, bool bWarnMissing );
bool VScript_RunCodeInScope( const char *pszCode, const char *pszSourceName, HSCRIPT hScope );

bool VScript_HasScopeFunction( HSCRIPT hScope, const char *pszFunc );
bool VScript_CallScopeFunction( HSCRIPT hScope, const char *pszFunc, CBaseEntity *pSelf,
							   int nArgs, const ScriptArg_t *pArgs, bool bWarnMissing,
							   float *pflRet = NULL );

// Game event payload, presented to scripts as a table of key/value pairs.
class IScriptEventAccess
{
public:
	virtual ~IScriptEventAccess() {}
	virtual const char *Name() const = 0;
	virtual int Count() const = 0;
	virtual const char *Key( int i ) const = 0;
	virtual int Type( int i ) const = 0;
	virtual int Int( int i ) const = 0;
	virtual float Float( int i ) const = 0;
	virtual bool Bool( int i ) const = 0;
	virtual const char *String( int i ) const = 0;
};

bool VScript_CallEventCallback( HSCRIPT hScope, const char *pszFunc, IScriptEventAccess &event );

// Walks a scope table and registers OnGameEvent_<name> callbacks through
// VScript_OnCollectGameEventCallback().
void VScript_CollectGameEventCallbacks( HSCRIPT hScope );

// Called by the VM when a scope declares an OnGameEvent_<name> callback.
void VScript_OnCollectGameEventCallback( const char *pszEventName, HSCRIPT hScope );

//-----------------------------------------------------------------------------
// Entity integration. These are called from the stock entity code paths and
// must stay cheap when scripting is off.
//-----------------------------------------------------------------------------
bool VScript_HandleEntityKeyValue( CBaseEntity *pEntity, const char *pszKey, const char *pszValue );
void VScript_EntitySpawned( CBaseEntity *pEntity );
void VScript_EntityRemoved( CBaseEntity *pEntity );
void VScript_AddThinkToEntity( CBaseEntity *pEntity, const char *pszFunc, float flDelay );

// Queue an entity input. Zero-delay inputs are serviced after entity thinks,
// exactly like meet-the-engine map logic, so scripts cannot reenter themselves.
void VScript_QueueEntFire( CBaseEntity *pTarget, const char *pszTargetName, const char *pszInput,
						   const char *pszValue, float flDelay, CBaseEntity *pActivator, CBaseEntity *pCaller );

int VScript_ListenToGameEvent( const char *pszEvent, HSCRIPT hScope, const char *pszFunc );
void VScript_StopListeningToGameEvent( int hListener );

HSCRIPT VScript_GetCurrentScope();
void VScript_Log( const char *pszFormat, ... );

//-----------------------------------------------------------------------------
// Gameplay hooks.
//-----------------------------------------------------------------------------
void VScript_OnPlayerSpawn( CBasePlayer *pPlayer );
void VScript_OnPlayerKilled( CBasePlayer *pVictim, CBaseEntity *pAttacker, const char *pszWeapon );

#endif // GAME_DLL
#endif // VSCRIPT_H
