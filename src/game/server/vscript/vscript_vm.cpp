//========= Copyright (c) 2026, improved-srcds-v34 contributors ============//
//
// Purpose: The Squirrel-backed VScript virtual machine. This is the only file
//          that includes squirrel.h; everything else talks to it through
//          vscript.h.
//
//          It owns: VM lifetime, the script handle table (entity handles and
//          scopes), scope creation, compiling and running files/code, calling
//          script functions from C++, and the native binding dispatch that
//          turns script calls into IScriptCallContext calls.
//
//=============================================================================//

#include "cbase.h"
#include "filesystem.h"
#include "tier1/utlbuffer.h"
#include "utlvector.h"
#include "vscript.h"

#include "squirrel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Convars. All of them default to stock behaviour: nothing runs until an admin
// turns scripting on.
//-----------------------------------------------------------------------------
ConVar sv_vscript_enable( "sv_vscript_enable", "0", FCVAR_NOTIFY,
	"Enable the server-side VScript subsystem (0 = stock CS:S behaviour)" );
ConVar sv_vscript_debug( "sv_vscript_debug", "0", FCVAR_NOTIFY,
	"Log every VScript binding call" );
ConVar sv_vscript_warn_ms( "sv_vscript_warn_ms", "50", FCVAR_NOTIFY,
	"Warn when a single script call takes longer than this many milliseconds" );
ConVar sv_vscript_allow_convars( "sv_vscript_allow_convars", "0", FCVAR_NOTIFY,
	"Allow scripts to change convars through Convars.SetValue" );

//-----------------------------------------------------------------------------
// Handles
//-----------------------------------------------------------------------------
enum VScriptHandleType_t
{
	VS_HANDLE_NONE = 0,
	VS_HANDLE_ENTITY,
	VS_HANDLE_SCOPE
};

struct VScriptHandle_t
{
	int iType;
	CHandle<CBaseEntity> hEntity;
	HSQOBJECT obj;
};

namespace
{
	HSQUIRRELVM			g_pVM = NULL;
	bool				g_bActive = false;
	bool				g_bInFrame = false;

	CUtlVector<VScriptHandle_t>	g_Handles;

	// entindex -> entity handle, so a script that asks for the same entity twice
	// gets the same script object (and the same identity comparisons).
	HSCRIPT				g_EntityHandles[MAX_EDICTS];

	HSQOBJECT			g_EntityClass;
	HSQOBJECT			g_VectorClass;
	bool				g_bClassesValid = false;

		int				g_nCallDepth = 0;
	int				g_nMaxCallDepth = 32;
	float				g_flWarnMs = 50.0f;
	HSCRIPT				g_hCurrentScope = 0;
}

void VScript_Log( const char *pszFormat, ... )
{
	char szBuffer[1024];
	va_list argptr;
	va_start( argptr, pszFormat );
	Q_vsnprintf( szBuffer, sizeof( szBuffer ), pszFormat, argptr );
	va_end( argptr );

	Warning( "[VScript] %s\n", szBuffer );
}

//-----------------------------------------------------------------------------
// Logging
//-----------------------------------------------------------------------------
void VScript_Warn( const char *pszFormat, ... )
{
	char szBuffer[1024];
	va_list argptr;
	va_start( argptr, pszFormat );
	Q_vsnprintf( szBuffer, sizeof( szBuffer ), pszFormat, argptr );
	va_end( argptr );

	Warning( "[VScript] %s\n", szBuffer );
}

static void VScript_PrintFunc( HSQUIRRELVM vm, const SQChar *s, ... )
{
	char szBuffer[1024];
	va_list argptr;
	va_start( argptr, s );
	Q_vsnprintf( szBuffer, sizeof( szBuffer ), s, argptr );
	va_end( argptr );

	Msg( "%s", szBuffer );
}

static void VScript_ErrorFunc( HSQUIRRELVM vm, const SQChar *s, ... )
{
	char szBuffer[1024];
	va_list argptr;
	va_start( argptr, s );
	Q_vsnprintf( szBuffer, sizeof( szBuffer ), s, argptr );
	va_end( argptr );

	Warning( "[VScript] %s", szBuffer );
}

static void VScript_CompilerError( HSQUIRRELVM vm, const SQChar *pszDesc, const SQChar *pszSource,
								   SQInteger line, SQInteger column )
{
	Warning( "[VScript] %s:%d:%d: %s\n", pszSource ? pszSource : "?", (int)line, (int)column,
			 pszDesc ? pszDesc : "compile error" );
}

//-----------------------------------------------------------------------------
// Small stack helpers
//-----------------------------------------------------------------------------
static VScriptHandle_t *HandleAt( HSCRIPT h )
{
	if ( h <= 0 || h >= g_Handles.Count() )
		return NULL;
	return &g_Handles[h];
}

static void ResetHandle( VScriptHandle_t *pHandle )
{
	if ( !pHandle )
		return;

	if ( g_pVM && pHandle->obj._unVal.pTable )
		sq_release( g_pVM, &pHandle->obj );
	sq_resetobject( &pHandle->obj );
	pHandle->iType = VS_HANDLE_NONE;
	pHandle->hEntity = NULL;
}

static HSCRIPT AllocHandle( int iType, CBaseEntity *pOwner )
{
	VScriptHandle_t handle;
	handle.iType = iType;
	handle.hEntity = pOwner;
	sq_resetobject( &handle.obj );
	return g_Handles.AddToTail( handle );
}

static void PushScopeObject( HSCRIPT hScope )
{
	if ( hScope )
	{
		VScriptHandle_t *pHandle = HandleAt( hScope );
		if ( pHandle && pHandle->iType == VS_HANDLE_SCOPE )
		{
			sq_pushobject( g_pVM, pHandle->obj );
			return;
		}
	}
	sq_pushroottable( g_pVM );
}

//-----------------------------------------------------------------------------
// Pushing script values
//-----------------------------------------------------------------------------
static HSCRIPT GetEntityHandle( CBaseEntity *pEntity );

static void PushVector( const Vector &vecValue )
{
	if ( !g_bClassesValid )
	{
		sq_pushnull( g_pVM );
		return;
	}

	sq_pushobject( g_pVM, g_VectorClass );
	sq_createinstance( g_pVM, -1 );
	sq_remove( g_pVM, -2 );		// class

	sq_pushstring( g_pVM, "x", -1 );
	sq_pushfloat( g_pVM, vecValue.x );
	sq_rawset( g_pVM, -3 );

	sq_pushstring( g_pVM, "y", -1 );
	sq_pushfloat( g_pVM, vecValue.y );
	sq_rawset( g_pVM, -3 );

	sq_pushstring( g_pVM, "z", -1 );
	sq_pushfloat( g_pVM, vecValue.z );
	sq_rawset( g_pVM, -3 );
}

static void PushEntityInstance( CBaseEntity *pEntity )
{
	HSCRIPT h = GetEntityHandle( pEntity );
	VScriptHandle_t *pHandle = HandleAt( h );
	if ( pHandle )
		sq_pushobject( g_pVM, pHandle->obj );
	else
		sq_pushnull( g_pVM );
}

static void PushArg( const ScriptArg_t &arg )
{
	switch ( arg.iType )
	{
	case SCRIPT_INT:	sq_pushinteger( g_pVM, arg.iValue ); break;
	case SCRIPT_FLOAT:	sq_pushfloat( g_pVM, arg.flValue ); break;
	case SCRIPT_BOOL:	sq_pushbool( g_pVM, arg.bValue ? SQTrue : SQFalse ); break;
	case SCRIPT_STRING:	sq_pushstring( g_pVM, arg.strValue.Get(), -1 ); break;
	case SCRIPT_VECTOR:	PushVector( arg.vecValue ); break;
	case SCRIPT_ENTITY:	PushEntityInstance( (CBaseEntity *)(long)arg.iValue ); break;
	default:			sq_pushnull( g_pVM ); break;
	}
}

//-----------------------------------------------------------------------------
// Reading script values off the stack
//-----------------------------------------------------------------------------
static CBaseEntity *EntityFromStack( int idx )
{
	if ( sq_gettype( g_pVM, idx ) != OT_INSTANCE )
		return NULL;

	SQUserPointer pUser = NULL;
	if ( SQ_FAILED( sq_getinstanceup( g_pVM, idx, &pUser, 0 ) ) || !pUser )
		return NULL;

	VScriptHandle_t *pHandle = HandleAt( (HSCRIPT)(long)pUser );
	if ( !pHandle || pHandle->iType != VS_HANDLE_ENTITY )
		return NULL;

	return pHandle->hEntity.Get();
}

static Vector VectorFromStack( int idx, const Vector &vecDefault )
{
	Vector vecResult = vecDefault;

	if ( sq_gettype( g_pVM, idx ) != OT_INSTANCE && sq_gettype( g_pVM, idx ) != OT_TABLE )
		return vecResult;

	sq_pushstring( g_pVM, "x", -1 );
	if ( SQ_SUCCEEDED( sq_get( g_pVM, idx ) ) )
	{
		if ( sq_gettype( g_pVM, -1 ) == OT_INTEGER )
		{
			SQInteger n = 0;
			sq_getinteger( g_pVM, -1, &n );
			vecResult.x = (float)n;
		}
		else
		{
			SQFloat f = 0.0f;
			sq_getfloat( g_pVM, -1, &f );
			vecResult.x = (float)f;
		}
	}
	sq_pop( g_pVM, 1 );

	sq_pushstring( g_pVM, "y", -1 );
	if ( SQ_SUCCEEDED( sq_get( g_pVM, idx ) ) )
	{
		if ( sq_gettype( g_pVM, -1 ) == OT_INTEGER )
		{
			SQInteger n = 0;
			sq_getinteger( g_pVM, -1, &n );
			vecResult.y = (float)n;
		}
		else
		{
			SQFloat f = 0.0f;
			sq_getfloat( g_pVM, -1, &f );
			vecResult.y = (float)f;
		}
	}
	sq_pop( g_pVM, 1 );

	sq_pushstring( g_pVM, "z", -1 );
	if ( SQ_SUCCEEDED( sq_get( g_pVM, idx ) ) )
	{
		if ( sq_gettype( g_pVM, -1 ) == OT_INTEGER )
		{
			SQInteger n = 0;
			sq_getinteger( g_pVM, -1, &n );
			vecResult.z = (float)n;
		}
		else
		{
			SQFloat f = 0.0f;
			sq_getfloat( g_pVM, -1, &f );
			vecResult.z = (float)f;
		}
	}
	sq_pop( g_pVM, 1 );

	return vecResult;
}

//-----------------------------------------------------------------------------
// Logging the error object left on the stack by a failed sq_call.
//-----------------------------------------------------------------------------
static void LogStackError( const char *pszContext )
{
	if ( sq_gettop( g_pVM ) < 1 )
	{
		VScript_Warn( "%s: script error", pszContext );
		return;
	}

	const SQChar *pszError = NULL;
	if ( SQ_FAILED( sq_tostring( g_pVM, -1 ) ) || SQ_FAILED( sq_getstring( g_pVM, -1, &pszError ) ) )
	{
		sq_pop( g_pVM, 1 );
		VScript_Warn( "%s: script error", pszContext );
		return;
	}

	VScript_Warn( "%s: %s", pszContext, pszError ? pszError : "script error" );
	sq_pop( g_pVM, 1 );

	// Best effort stack trace, one line per frame.
	for ( int i = 0; i < 3; i++ )
	{
		SQStackInfos si;
		if ( SQ_FAILED( sq_stackinfos( g_pVM, i, &si ) ) )
			break;
		if ( !si.funcname )
			continue;
		VScript_Warn( "  at %s (%s:%d)", si.funcname, si.source ? si.source : "?", (int)si.line );
	}
}

//-----------------------------------------------------------------------------
// Handle creation
//-----------------------------------------------------------------------------
static HSCRIPT GetEntityHandle( CBaseEntity *pEntity )
{
	if ( !g_pVM || !pEntity )
		return 0;

	int iEntIndex = pEntity->entindex();
	if ( iEntIndex >= 0 && iEntIndex < MAX_EDICTS )
	{
		HSCRIPT hCached = g_EntityHandles[iEntIndex];
		VScriptHandle_t *pCached = HandleAt( hCached );
		if ( pCached && pCached->iType == VS_HANDLE_ENTITY && pCached->hEntity.Get() == pEntity )
			return hCached;
	}

	if ( !g_bClassesValid )
		return 0;

	HSCRIPT hNew = AllocHandle( VS_HANDLE_ENTITY, pEntity );

	sq_pushobject( g_pVM, g_EntityClass );
	if ( SQ_FAILED( sq_createinstance( g_pVM, -1 ) ) )
	{
		sq_pop( g_pVM, 1 );
		return 0;
	}
	sq_remove( g_pVM, -2 );

	sq_setinstanceup( g_pVM, -1, (SQUserPointer)(long)hNew );

	VScriptHandle_t *pHandle = HandleAt( hNew );
	sq_getstackobj( g_pVM, -1, &pHandle->obj );
	sq_addref( g_pVM, &pHandle->obj );
	sq_pop( g_pVM, 1 );

	if ( iEntIndex >= 0 && iEntIndex < MAX_EDICTS )
		g_EntityHandles[iEntIndex] = hNew;

	return hNew;
}

//-----------------------------------------------------------------------------
// Native dispatch. Every binding closure carries its table index as a free
// variable; Squirrel places free variables at the top of the native frame, so
// the index is the object just past the arguments.
//-----------------------------------------------------------------------------
static int BindingIndexFromStack( HSQUIRRELVM vm )
{
	int iTop = (int)sq_gettop( vm );
	SQInteger nIndex = -1;
	if ( SQ_FAILED( sq_getinteger( vm, iTop, &nIndex ) ) )
		return -1;
	return (int)nIndex;
}

class CVScriptCallContext : public IScriptCallContext
{
public:
	CVScriptCallContext( HSQUIRRELVM vm, const char *pszName )
		: m_pVM( vm ), m_pszName( pszName ), m_bReturned( false ), m_bSelfResolved( false ),
		  m_pSelf( NULL )
	{
	}

	virtual int Count() const
	{
		int iTop = (int)sq_gettop( m_pVM );
		int nCount = iTop - 2;
		return nCount > 0 ? nCount : 0;
	}

	virtual int ArgType( int i ) const
	{
		if ( !Exists( i ) )
			return SCRIPT_VOID;

		switch ( sq_gettype( m_pVM, i + 2 ) )
		{
		case OT_INTEGER:	return SCRIPT_INT;
		case OT_FLOAT:		return SCRIPT_FLOAT;
		case OT_BOOL:		return SCRIPT_BOOL;
		case OT_STRING:		return SCRIPT_STRING;
		case OT_INSTANCE:	return SCRIPT_ENTITY;
		default:			return SCRIPT_VOID;
		}
	}

	virtual bool Exists( int i ) const
	{
		return i >= 0 && i < Count();
	}

	virtual int ArgInt( int i, int nDefault = 0 ) const
	{
		if ( !Exists( i ) )
			return nDefault;

		int idx = i + 2;
		SQObjectType type = sq_gettype( m_pVM, idx );

		if ( type == OT_INTEGER )
		{
			SQInteger n = 0;
			sq_getinteger( m_pVM, idx, &n );
			return (int)n;
		}
		if ( type == OT_FLOAT )
		{
			SQFloat f = 0.0f;
			sq_getfloat( m_pVM, idx, &f );
			return (int)f;
		}
		if ( type == OT_BOOL )
		{
			SQBool b = SQFalse;
			sq_getbool( m_pVM, idx, &b );
			return b ? 1 : 0;
		}
		return nDefault;
	}

	virtual float ArgFloat( int i, float flDefault = 0.0f ) const
	{
		if ( !Exists( i ) )
			return flDefault;

		int idx = i + 2;
		SQObjectType type = sq_gettype( m_pVM, idx );

		if ( type == OT_FLOAT )
		{
			SQFloat f = 0.0f;
			sq_getfloat( m_pVM, idx, &f );
			return (float)f;
		}
		if ( type == OT_INTEGER )
		{
			SQInteger n = 0;
			sq_getinteger( m_pVM, idx, &n );
			return (float)n;
		}
		return flDefault;
	}

	virtual bool ArgBool( int i, bool bDefault = false ) const
	{
		if ( !Exists( i ) )
			return bDefault;

		int idx = i + 2;
		SQObjectType type = sq_gettype( m_pVM, idx );

		if ( type == OT_BOOL )
		{
			SQBool b = SQFalse;
			sq_getbool( m_pVM, idx, &b );
			return b ? true : false;
		}
		if ( type == OT_INTEGER )
			return ArgInt( i, 0 ) != 0;

		return bDefault;
	}

	virtual const char *ArgString( int i, const char *pszDefault = "" ) const
	{
		if ( !Exists( i ) )
			return pszDefault;

		int idx = i + 2;
		if ( sq_gettype( m_pVM, idx ) != OT_STRING )
			return pszDefault;

		const SQChar *pszValue = NULL;
		if ( SQ_FAILED( sq_getstring( m_pVM, idx, &pszValue ) ) || !pszValue )
			return pszDefault;

		return pszValue;
	}

	virtual Vector ArgVector( int i, const Vector &vecDefault = vec3_origin ) const
	{
		if ( !Exists( i ) )
			return vecDefault;
		return VectorFromStack( i + 2, vecDefault );
	}

	virtual CBaseEntity *ArgEntity( int i ) const
	{
		if ( !Exists( i ) )
			return NULL;
		return EntityFromStack( i + 2 );
	}

	virtual CBaseEntity *Self() const
	{
		if ( !m_bSelfResolved )
		{
			m_bSelfResolved = true;

			SQUserPointer pUser = NULL;
			if ( SQ_SUCCEEDED( sq_getinstanceup( m_pVM, 1, &pUser, 0 ) ) && pUser )
			{
				VScriptHandle_t *pHandle = HandleAt( (HSCRIPT)(long)pUser );
				if ( pHandle && pHandle->iType == VS_HANDLE_ENTITY )
					m_pSelf = pHandle->hEntity.Get();
			}
		}
		return m_pSelf;
	}

	virtual void ReturnNull() { sq_pushnull( m_pVM ); m_bReturned = true; }
	virtual void ReturnInt( int nValue ) { sq_pushinteger( m_pVM, nValue ); m_bReturned = true; }
	virtual void ReturnFloat( float flValue ) { sq_pushfloat( m_pVM, flValue ); m_bReturned = true; }
	virtual void ReturnBool( bool bValue ) { sq_pushbool( m_pVM, bValue ? SQTrue : SQFalse ); m_bReturned = true; }

	virtual void ReturnString( const char *pszValue )
	{
		if ( pszValue )
			sq_pushstring( m_pVM, pszValue, -1 );
		else
			sq_pushnull( m_pVM );
		m_bReturned = true;
	}

	virtual void ReturnVector( const Vector &vecValue )
	{
		PushVector( vecValue );
		m_bReturned = true;
	}

	virtual void ReturnEntity( CBaseEntity *pEntity )
	{
		if ( pEntity )
			PushEntityInstance( pEntity );
		else
			sq_pushnull( m_pVM );
		m_bReturned = true;
	}

	virtual void ReturnScope( HSCRIPT hScope )
	{
		PushScopeObject( hScope );
		m_bReturned = true;
	}

	virtual void Error( const char *pszFormat, ... )
	{
		char szBuffer[512];
		va_list argptr;
		va_start( argptr, pszFormat );
		Q_vsnprintf( szBuffer, sizeof( szBuffer ), pszFormat, argptr );
		va_end( argptr );

		VScript_Warn( "%s: %s", m_pszName ? m_pszName : "binding", szBuffer );
	}

	int ReturnCount() const { return m_bReturned ? 1 : 0; }

private:
	HSQUIRRELVM	m_pVM;
	const char *m_pszName;
	bool		m_bReturned;
	mutable bool m_bSelfResolved;
	mutable CBaseEntity *m_pSelf;
};

static SQInteger GlobalDispatch( HSQUIRRELVM vm )
{
	int iBinding = BindingIndexFromStack( vm );
	if ( iBinding < 0 || iBinding >= g_nScriptGlobalBindings )
		return 0;

	CVScriptCallContext ctx( vm, g_ScriptGlobalBindings[iBinding].pszName );
	g_ScriptGlobalBindings[iBinding].pfn( ctx );
	return ctx.ReturnCount();
}

static SQInteger EntityDispatch( HSQUIRRELVM vm )
{
	int iBinding = BindingIndexFromStack( vm );
	if ( iBinding < 0 || iBinding >= g_nScriptEntityBindings )
		return 0;

	CVScriptCallContext ctx( vm, g_ScriptEntityBindings[iBinding].pszName );
	g_ScriptEntityBindings[iBinding].pfn( ctx );
	return ctx.ReturnCount();
}

static SQInteger EntityListDispatch( HSQUIRRELVM vm )
{
	int iBinding = BindingIndexFromStack( vm );
	if ( iBinding < 0 || iBinding >= g_nScriptEntityListBindings )
		return 0;

	CVScriptCallContext ctx( vm, g_ScriptEntityListBindings[iBinding].pszName );
	g_ScriptEntityListBindings[iBinding].pfn( ctx );
	return ctx.ReturnCount();
}

static SQInteger ConvarDispatch( HSQUIRRELVM vm )
{
	int iBinding = BindingIndexFromStack( vm );
	if ( iBinding < 0 || iBinding >= g_nScriptConvarBindings )
		return 0;

	CVScriptCallContext ctx( vm, g_ScriptConvarBindings[iBinding].pszName );
	g_ScriptConvarBindings[iBinding].pfn( ctx );
	return ctx.ReturnCount();
}

static SQInteger NetPropDispatch( HSQUIRRELVM vm )
{
	int iBinding = BindingIndexFromStack( vm );
	if ( iBinding < 0 || iBinding >= g_nScriptNetPropBindings )
		return 0;

	CVScriptCallContext ctx( vm, g_ScriptNetPropBindings[iBinding].pszName );
	g_ScriptNetPropBindings[iBinding].pfn( ctx );
	return ctx.ReturnCount();
}

static SQInteger VectorDispatch( HSQUIRRELVM vm )
{
	int iBinding = BindingIndexFromStack( vm );
	if ( iBinding < 0 || iBinding >= g_nScriptVectorBindings )
		return 0;

	CVScriptCallContext ctx( vm, g_ScriptVectorBindings[iBinding].pszName );
	g_ScriptVectorBindings[iBinding].pfn( ctx );
	return ctx.ReturnCount();
}

//-----------------------------------------------------------------------------
// Registration helpers
//-----------------------------------------------------------------------------
static void PushBinding( SQFUNCTION dispatch, const ScriptBinding_t &binding, int iIndex )
{
	sq_pushstring( g_pVM, binding.pszName, -1 );
	sq_newclosure( g_pVM, dispatch, 1 );
	sq_pushinteger( g_pVM, iIndex );
	sq_setfreevariable( g_pVM, -2, 0 );
	sq_setnativeclosurename( g_pVM, -1, binding.pszName );
}

static void RegisterClass( const char *pszName, const ScriptBinding_t *pBindings, int nBindings,
						   SQFUNCTION dispatch, HSQOBJECT *pOutClass )
{
	sq_pushroottable( g_pVM );
	sq_pushstring( g_pVM, pszName, -1 );
	sq_newclass( g_pVM, SQFalse );

	for ( int i = 0; i < nBindings; i++ )
	{
		PushBinding( dispatch, pBindings[i], i );
		sq_newslot( g_pVM, -3, SQFalse );
	}

	if ( pOutClass )
	{
		sq_getstackobj( g_pVM, -1, pOutClass );
		sq_addref( g_pVM, pOutClass );
	}

	sq_newslot( g_pVM, -3, SQFalse );
	sq_pop( g_pVM, 1 );
}

static void RegisterTable( const char *pszName, const ScriptBinding_t *pBindings, int nBindings,
						   SQFUNCTION dispatch )
{
	sq_pushroottable( g_pVM );
	sq_pushstring( g_pVM, pszName, -1 );
	sq_newtable( g_pVM );

	for ( int i = 0; i < nBindings; i++ )
	{
		PushBinding( dispatch, pBindings[i], i );
		sq_newslot( g_pVM, -3, SQFalse );
	}

	sq_newslot( g_pVM, -3, SQFalse );
	sq_pop( g_pVM, 1 );
}

static void RegisterGlobals()
{
	sq_pushroottable( g_pVM );
	for ( int i = 0; i < g_nScriptGlobalBindings; i++ )
	{
		PushBinding( GlobalDispatch, g_ScriptGlobalBindings[i], i );
		sq_newslot( g_pVM, -3, SQFalse );
	}
	sq_pop( g_pVM, 1 );
}

//-----------------------------------------------------------------------------
// VM lifetime
//-----------------------------------------------------------------------------
bool VScript_IsActive()
{
	return g_bActive && g_pVM != NULL;
}

bool VScript_Init()
{
	if ( !sv_vscript_enable.GetBool() )
		return false;

	if ( g_pVM )
		VScript_Shutdown();

	g_Handles.RemoveAll();
	g_Handles.AddToTail( VScriptHandle_t() );
	sq_resetobject( &g_EntityClass );
	sq_resetobject( &g_VectorClass );
	g_bClassesValid = false;
	g_nCallDepth = 0;

	for ( int i = 0; i < MAX_EDICTS; i++ )
		g_EntityHandles[i] = 0;

	g_pVM = sq_open( 1024 );
	if ( !g_pVM )
	{
		VScript_Warn( "failed to create the Squirrel VM" );
		return false;
	}

	sq_setprintfunc( g_pVM, VScript_PrintFunc, VScript_ErrorFunc );
	sq_setcompilererrorhandler( g_pVM, VScript_CompilerError );
	sq_enabledebuginfo( g_pVM, SQTrue );

	RegisterClass( "CBaseEntity", g_ScriptEntityBindings, g_nScriptEntityBindings,
				   EntityDispatch, &g_EntityClass );
	RegisterClass( "Vector", g_ScriptVectorBindings, g_nScriptVectorBindings,
				   VectorDispatch, &g_VectorClass );
	g_bClassesValid = true;

	RegisterTable( "Entities", g_ScriptEntityListBindings, g_nScriptEntityListBindings, EntityListDispatch );
	RegisterTable( "Convars", g_ScriptConvarBindings, g_nScriptConvarBindings, ConvarDispatch );
	RegisterTable( "NetProps", g_ScriptNetPropBindings, g_nScriptNetPropBindings, NetPropDispatch );
	RegisterGlobals();

	g_bActive = true;
	g_flWarnMs = sv_vscript_warn_ms.GetFloat();

	VScript_Warn( "Squirrel %d initialized (sv_vscript_enable 1)", (int)sq_getversion() );
	return true;
}

void VScript_Shutdown()
{
	if ( !g_pVM )
	{
		g_bActive = false;
		return;
	}

	g_bActive = false;

	for ( int i = 0; i < g_Handles.Count(); i++ )
		ResetHandle( &g_Handles[i] );
	g_Handles.RemoveAll();

	if ( g_bClassesValid )
	{
		sq_release( g_pVM, &g_EntityClass );
		sq_release( g_pVM, &g_VectorClass );
	}
	g_bClassesValid = false;
	sq_resetobject( &g_EntityClass );
	sq_resetobject( &g_VectorClass );

	sq_close( g_pVM );
	g_pVM = NULL;

	for ( int i = 0; i < MAX_EDICTS; i++ )
		g_EntityHandles[i] = 0;
}

//-----------------------------------------------------------------------------
// Scopes
//-----------------------------------------------------------------------------
HSCRIPT VScript_CreateScope( CBaseEntity *pOwner )
{
	if ( !g_pVM )
		return 0;

	sq_newtable( g_pVM );
	sq_pushroottable( g_pVM );
	sq_setdelegate( g_pVM, -2 );

	HSCRIPT hScope = AllocHandle( VS_HANDLE_SCOPE, pOwner );

	VScriptHandle_t *pHandle = HandleAt( hScope );
	sq_getstackobj( g_pVM, -1, &pHandle->obj );
	sq_addref( g_pVM, &pHandle->obj );
	sq_pop( g_pVM, 1 );

	if ( pOwner )
		VScript_SetScopeEntity( hScope, "self", pOwner );

	return hScope;
}

void VScript_DestroyScope( HSCRIPT hScope )
{
	VScriptHandle_t *pHandle = HandleAt( hScope );
	if ( !pHandle || pHandle->iType != VS_HANDLE_SCOPE )
		return;

	ResetHandle( pHandle );
}

HSCRIPT VScript_GetOwnerScope( CBaseEntity *pEntity )
{
	if ( !g_pVM || !pEntity )
		return 0;

	for ( int i = 1; i < g_Handles.Count(); i++ )
	{
		VScriptHandle_t *pHandle = &g_Handles[i];
		if ( pHandle->iType == VS_HANDLE_SCOPE && pHandle->hEntity.Get() == pEntity )
			return i;
	}
	return 0;
}

void VScript_SetScopeEntity( HSCRIPT hScope, const char *pszKey, CBaseEntity *pEntity )
{
	VScriptHandle_t *pHandle = HandleAt( hScope );
	if ( !pHandle || pHandle->iType != VS_HANDLE_SCOPE )
		return;

	sq_pushobject( g_pVM, pHandle->obj );	// [scope]
	sq_pushstring( g_pVM, pszKey, -1 );		// [scope,key]
	PushEntityInstance( pEntity );			// [scope,key,value]
	sq_rawset( g_pVM, -3 );
	sq_pop( g_pVM, 1 );
}

void VScript_SetScopeString( HSCRIPT hScope, const char *pszKey, const char *pszValue )
{
	VScriptHandle_t *pHandle = HandleAt( hScope );
	if ( !pHandle || pHandle->iType != VS_HANDLE_SCOPE )
		return;

	sq_pushobject( g_pVM, pHandle->obj );
	sq_pushstring( g_pVM, pszKey, -1 );
	sq_pushstring( g_pVM, pszValue ? pszValue : "", -1 );
	sq_rawset( g_pVM, -3 );
	sq_pop( g_pVM, 1 );
}

void VScript_SetScopeInt( HSCRIPT hScope, const char *pszKey, int nValue )
{
	VScriptHandle_t *pHandle = HandleAt( hScope );
	if ( !pHandle || pHandle->iType != VS_HANDLE_SCOPE )
		return;

	sq_pushobject( g_pVM, pHandle->obj );
	sq_pushstring( g_pVM, pszKey, -1 );
	sq_pushinteger( g_pVM, nValue );
	sq_rawset( g_pVM, -3 );
	sq_pop( g_pVM, 1 );
}

//-----------------------------------------------------------------------------
// Files
//-----------------------------------------------------------------------------
static bool BuildScriptPath( const char *pszFile, char *pszOut, int nOutSize )
{
	if ( !pszFile || !pszFile[0] )
		return false;

	if ( !V_strnicmp( pszFile, "scripts/", 8 ) )
	{
		Q_strncpy( pszOut, pszFile, nOutSize );
	}
	else
	{
		Q_snprintf( pszOut, nOutSize, "scripts/vscripts/%s", pszFile );
	}

	// VScript appends .nut when the caller did not give an extension.
	const char *pszDot = strrchr( pszOut, '.' );
	const char *pszSlash = strrchr( pszOut, '/' );
	if ( !pszDot || ( pszSlash && pszDot < pszSlash ) )
	{
		Q_strncat( pszOut, ".nut", nOutSize, COPY_ALL_CHARACTERS );
	}
	return true;
}

static bool ReadScriptFile( const char *pszFile, CUtlBuffer &buffer )
{
	char szPath[MAX_PATH];
	if ( !BuildScriptPath( pszFile, szPath, sizeof( szPath ) ) )
		return false;

	if ( !filesystem->ReadFile( szPath, "GAME", buffer, 0 ) )
		return false;

	return buffer.TellMaxPut() > 0;
}

bool VScript_RunFileInScope( const char *pszFile, HSCRIPT hScope, bool bWarnMissing )
{
	if ( !VScript_IsActive() || !pszFile )
		return false;

	CUtlBuffer buffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( !ReadScriptFile( pszFile, buffer ) )
	{
		if ( bWarnMissing )
			VScript_Warn( "script not found: %s", pszFile );
		return false;
	}

	char szName[MAX_PATH];
	BuildScriptPath( pszFile, szName, sizeof( szName ) );

	return VScript_RunCodeInScope( (const char *)buffer.PeekGet(), szName, hScope );
}

bool VScript_RunCodeInScope( const char *pszCode, const char *pszSourceName, HSCRIPT hScope )
{
	if ( !VScript_IsActive() || !pszCode )
		return false;

	int iTop = sq_gettop( g_pVM );
	const char *pszName = pszSourceName ? pszSourceName : "RunScriptCode";

	if ( SQ_FAILED( sq_compilebuffer( g_pVM, pszCode, (SQInteger)Q_strlen( pszCode ), pszName, SQFalse ) ) )
	{
		sq_settop( g_pVM, iTop );
		VScript_Warn( "%s: compile failed", pszName );
		return false;
	}

	PushScopeObject( hScope );

	HSCRIPT hPreviousScope = g_hCurrentScope;
	g_hCurrentScope = hScope;

	SQRESULT rc = sq_call( g_pVM, 1, SQFalse, SQFalse );

	g_hCurrentScope = hPreviousScope;

	if ( SQ_FAILED( rc ) )
	{
		LogStackError( pszName );
		sq_settop( g_pVM, iTop );
		return false;
	}

	sq_settop( g_pVM, iTop );
	return true;
}

HSCRIPT VScript_GetCurrentScope()
{
	return g_hCurrentScope;
}

//-----------------------------------------------------------------------------
// Calling script functions from C++
//-----------------------------------------------------------------------------
static bool LookupScopeFunction( HSCRIPT hScope, const char *pszFunc, int *pStackBase )
{
	int iTop = sq_gettop( g_pVM );

	PushScopeObject( hScope );
	int iScopeIdx = sq_gettop( g_pVM );

	sq_pushstring( g_pVM, pszFunc, -1 );
	if ( SQ_FAILED( sq_get( g_pVM, iScopeIdx ) ) )
	{
		sq_settop( g_pVM, iTop );
		return false;
	}

	SQObjectType type = sq_gettype( g_pVM, -1 );
	if ( type != OT_CLOSURE && type != OT_NATIVECLOSURE )
	{
		sq_settop( g_pVM, iTop );
		return false;
	}

	*pStackBase = iTop;
	return true;
}

bool VScript_HasScopeFunction( HSCRIPT hScope, const char *pszFunc )
{
	if ( !VScript_IsActive() || !pszFunc )
		return false;

	int iTop = 0;
	if ( !LookupScopeFunction( hScope, pszFunc, &iTop ) )
		return false;

	sq_settop( g_pVM, iTop );
	return true;
}

bool VScript_CallScopeFunction( HSCRIPT hScope, const char *pszFunc, CBaseEntity *pSelf,
							    int nArgs, const ScriptArg_t *pArgs, bool bWarnMissing,
							    float *pflRet )
{
	if ( !VScript_IsActive() || !pszFunc )
		return false;

	int iTop = 0;
	if ( !LookupScopeFunction( hScope, pszFunc, &iTop ) )
	{
		if ( bWarnMissing )
			VScript_Warn( "script function not found: %s", pszFunc );
		return false;
	}

	// `this`: the entity the script belongs to, else the scope table itself.
	VScriptHandle_t *pScope = HandleAt( hScope );
	CBaseEntity *pThis = pSelf;
	if ( !pThis && pScope )
		pThis = pScope->hEntity.Get();

	if ( pThis )
		PushEntityInstance( pThis );
	else
		PushScopeObject( hScope );

	for ( int i = 0; i < nArgs; i++ )
		PushArg( pArgs[i] );

	if ( g_nCallDepth >= g_nMaxCallDepth )
	{
		VScript_Warn( "%s: call depth limit reached (%d)", pszFunc, g_nMaxCallDepth );
		sq_settop( g_pVM, iTop );
		return false;
	}

	g_nCallDepth++;

	HSCRIPT hPreviousScope = g_hCurrentScope;
	g_hCurrentScope = hScope;

	SQRESULT rc = sq_call( g_pVM, nArgs + 1, pflRet ? SQTrue : SQFalse, SQFalse );

	g_hCurrentScope = hPreviousScope;
	g_nCallDepth--;

	if ( SQ_FAILED( rc ) )
	{
		LogStackError( pszFunc );
		sq_settop( g_pVM, iTop );
		return false;
	}

	if ( pflRet )
	{
		float flValue = 0.0f;
		SQObjectType type = sq_gettype( g_pVM, -1 );
		if ( type == OT_FLOAT )
		{
			SQFloat f = 0.0f;
			sq_getfloat( g_pVM, -1, &f );
			flValue = (float)f;
		}
		else if ( type == OT_INTEGER )
		{
			SQInteger n = 0;
			sq_getinteger( g_pVM, -1, &n );
			flValue = (float)n;
		}
		*pflRet = flValue;
	}

	sq_settop( g_pVM, iTop );
	return true;
}

//-----------------------------------------------------------------------------
// Game event callbacks: the payload is materialised as a script table so
// scripts read event.userid, event.attacker, ... exactly as documented.
//-----------------------------------------------------------------------------
bool VScript_CallEventCallback( HSCRIPT hScope, const char *pszFunc, IScriptEventAccess &event )
{
	if ( !VScript_IsActive() || !pszFunc )
		return false;

	int iTop = 0;
	if ( !LookupScopeFunction( hScope, pszFunc, &iTop ) )
		return false;

	VScriptHandle_t *pScope = HandleAt( hScope );
	if ( pScope )
		PushEntityInstance( pScope->hEntity.Get() );
	else
		PushScopeObject( hScope );

	// Payload table.
	sq_newtable( g_pVM );
	int iTableIdx = sq_gettop( g_pVM );

	sq_pushstring( g_pVM, "name", -1 );
	sq_pushstring( g_pVM, event.Name() ? event.Name() : "", -1 );
	sq_rawset( g_pVM, iTableIdx );

	for ( int i = 0; i < event.Count(); i++ )
	{
		const char *pszKey = event.Key( i );
		if ( !pszKey || !pszKey[0] )
			continue;

		sq_pushstring( g_pVM, pszKey, -1 );

		switch ( event.Type( i ) )
		{
		case SCRIPT_INT:	sq_pushinteger( g_pVM, event.Int( i ) ); break;
		case SCRIPT_FLOAT:	sq_pushfloat( g_pVM, event.Float( i ) ); break;
		case SCRIPT_BOOL:	sq_pushbool( g_pVM, event.Bool( i ) ? SQTrue : SQFalse ); break;
		case SCRIPT_STRING:	sq_pushstring( g_pVM, event.String( i ), -1 ); break;
		default:			sq_pushnull( g_pVM ); break;
		}

		sq_rawset( g_pVM, iTableIdx );
	}

	SQRESULT rc = sq_call( g_pVM, 2, SQFalse, SQFalse );
	if ( SQ_FAILED( rc ) )
	{
		LogStackError( pszFunc );
		sq_settop( g_pVM, iTop );
		return false;
	}

	sq_settop( g_pVM, iTop );
	return true;
}

//-----------------------------------------------------------------------------
// Game event callback collection: walk a scope table for OnGameEvent_<name>.
//-----------------------------------------------------------------------------
void VScript_CollectGameEventCallbacks( HSCRIPT hScope )
{
	if ( !VScript_IsActive() )
		return;

	int iTop = sq_gettop( g_pVM );

	PushScopeObject( hScope );
	if ( sq_gettype( g_pVM, -1 ) != OT_TABLE )
	{
		sq_settop( g_pVM, iTop );
		return;
	}

	sq_pushnull( g_pVM );			// iterator
	while ( SQ_SUCCEEDED( sq_next( g_pVM, -2 ) ) )
	{
		if ( sq_gettype( g_pVM, -2 ) == OT_STRING )
		{
			const SQChar *pszKey = NULL;
			if ( SQ_SUCCEEDED( sq_getstring( g_pVM, -2, &pszKey ) ) && pszKey )
			{
				static const char *s_pszPrefix = "OnGameEvent_";
				int nPrefix = (int)Q_strlen( s_pszPrefix );
				if ( !V_strnicmp( pszKey, s_pszPrefix, nPrefix ) && Q_strlen( pszKey ) > nPrefix )
					VScript_OnCollectGameEventCallback( pszKey + nPrefix, hScope );
			}
		}
		sq_pop( g_pVM, 1 );			// drop the value, keep the iterator
	}

	sq_settop( g_pVM, iTop );
}

//-----------------------------------------------------------------------------
// Per-frame upkeep
//-----------------------------------------------------------------------------
void VScript_Frame()
{
	if ( !VScript_IsActive() )
		return;

	g_flWarnMs = sv_vscript_warn_ms.GetFloat();

	// Drop handles for entities that no longer exist. A destroyed entity leaves
	// a NULL CHandle, so a script holding it sees IsValid() == false instead of
	// a pointer to whatever entity reuses the edict next.
	for ( int i = 1; i < g_Handles.Count(); i++ )
	{
		VScriptHandle_t *pHandle = &g_Handles[i];
		if ( pHandle->iType == VS_HANDLE_ENTITY && pHandle->hEntity.Get() == NULL )
			ResetHandle( pHandle );
	}
}
