//========= Copyright (c) 2026, improved-srcds-v34 contributors ============//
//
// Purpose: Client sprays are uploaded into the mod's downloads/ tree and are
//          decoded by every client when the server broadcasts a "Player Decal"
//          temp entity. A crafted VTF header makes that decode crash clients.
//          This validates the uploaded files server-side, quarantines the ones
//          that cannot be a sane decal, and blocks the broadcast while a bad
//          file exists. Nothing here changes the wire format or the client.
//
//=============================================================================//

#include "cbase.h"
#include "filesystem.h"
#include "igamesystem.h"
#include "tier1/utlstring.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlvector.h"
#include "spraycheck.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar sv_spray_check( "sv_spray_check", "1", FCVAR_NOTIFY, "Validate client-uploaded sprays under downloads/ and quarantine invalid ones" );
ConVar sv_spray_enable( "sv_spray_enable", "1", FCVAR_NOTIFY, "Allow player spray decals (0 stops the Player Decal broadcast)" );

namespace
{
	const int SPRAY_SCAN_FILES_PER_FRAME = 32;
	const int SPRAY_SCAN_MAX_DEPTH = 3;
	const int SPRAY_HEADER_BYTES = 256;

	// VTF flags that a decal has no use for. RENDERTARGET, DEPTHRENDERTARGET,
	// NODEPTHBUFFER and BORDER are the ones the crash sprays set.
	const unsigned int SPRAY_BAD_FLAGS = 0x00008000u | 0x00010000u | 0x00080000u | 0x00800000u;

	inline unsigned short SprayU16( const unsigned char *p )
	{
		return (unsigned short)( p[0] | ( p[1] << 8 ) );
	}

	inline unsigned int SprayU32( const unsigned char *p )
	{
		return (unsigned int)p[0] | ( (unsigned int)p[1] << 8 ) |
			( (unsigned int)p[2] << 16 ) | ( (unsigned int)p[3] << 24 );
	}

	// Bytes per pixel, or bytes per 4x4 block for the compressed formats.
	bool SprayFormatBpp( unsigned int fmt, int &nBpp, bool &bCompressed )
	{
		switch ( fmt )
		{
		case 0: case 1: case 12: case 16:		// RGBA8888 / ABGR8888 / BGRA8888 / BGRX8888
			nBpp = 32; bCompressed = false; return true;
		case 2: case 3:							// RGB888 / BGR888
			nBpp = 24; bCompressed = false; return true;
		case 4: case 6:							// RGB565 / IA88
			nBpp = 16; bCompressed = false; return true;
		case 5:									// I8
			nBpp = 8; bCompressed = false; return true;
		case 13: case 20:						// DXT1 / DXT1_ONEBITALPHA
			nBpp = 8; bCompressed = true; return true;
		case 14: case 15:						// DXT3 / DXT5
			nBpp = 16; bCompressed = true; return true;
		default:
			return false;						// unknown format: skip the size check
		}
	}

	bool SprayImageBytes( unsigned int fmt, int w, int h, unsigned int &nBytes )
	{
		int nBpp;
		bool bCompressed;
		if ( w <= 0 || h <= 0 || !SprayFormatBpp( fmt, nBpp, bCompressed ) )
			return false;

		if ( bCompressed )
			nBytes = (unsigned int)( ( ( w + 3 ) / 4 ) * ( ( h + 3 ) / 4 ) * nBpp );
		else
			nBytes = (unsigned int)( ( w * h * nBpp ) / 8 );
		return true;
	}

	// Size of the header, the low-res image and every high-res mip, frame,
	// face and depth slice. Used only as a sanity check against the file size.
	bool SprayComputeTotalSize( const unsigned char *p, int n, double &flTotal )
	{
		if ( n < 80 )
			return false;

		const unsigned int nHeaderSize = SprayU32( p + 12 );
		const int w = SprayU16( p + 16 );
		const int h = SprayU16( p + 18 );
		const unsigned int nFlags = SprayU32( p + 20 );
		int nFrames = SprayU16( p + 24 );
		const unsigned int nFmt = SprayU32( p + 52 );
		const int nMips = p[56];
		const unsigned int nMinor = SprayU32( p + 8 );

		int nDepth = 1;
		if ( nMinor >= 2 )
			nDepth = SprayU16( p + 63 );
		if ( nDepth < 1 )
			nDepth = 1;

		const int nFaces = ( nFlags & 0x00004000u ) ? 6 : 1;	// ENVMAP
		if ( nFrames < 1 )
			nFrames = 1;
		if ( nMips < 1 || nMips > 16 )
			return false;

		flTotal = (double)nHeaderSize;

		const unsigned int nLowFmt = SprayU32( p + 57 );
		const int nLowW = p[61];
		const int nLowH = p[62];
		unsigned int nLowBytes;
		if ( nLowW > 0 && nLowH > 0 && SprayImageBytes( nLowFmt, nLowW, nLowH, nLowBytes ) )
			flTotal += (double)nLowBytes;

		for ( int i = 0; i < nMips; i++ )
		{
			int nw = w >> i;
			int nh = h >> i;
			if ( nw < 1 ) nw = 1;
			if ( nh < 1 ) nh = 1;

			unsigned int nBytes;
			if ( !SprayImageBytes( nFmt, nw, nh, nBytes ) )
				return false;

			flTotal += (double)nBytes * nFrames * nFaces * nDepth;
		}
		return true;
	}

	// Anything that cannot be a sane decal VTF is rejected. Unreadable files are
	// left alone: we only quarantine what we could actually inspect.
	bool SprayFileIsValid( const char *pszPath, const char **ppszReason )
	{
		CUtlBuffer buf;
		if ( !filesystem->ReadFile( pszPath, "GAME", buf, SPRAY_HEADER_BYTES ) )
			return true;

		const int n = buf.TellPut();
		if ( n < 80 )
		{
			*ppszReason = "truncated header";
			return false;
		}

		const unsigned char *p = (const unsigned char *)buf.Base();

		if ( p[0] != 'V' || p[1] != 'T' || p[2] != 'F' || p[3] != 0 )
		{
			*ppszReason = "not a VTF";
			return false;
		}

		const unsigned int nMajor = SprayU32( p + 4 );
		const unsigned int nMinor = SprayU32( p + 8 );
		if ( nMajor != 7 )
		{
			*ppszReason = "unsupported VTF version";
			return false;
		}
		if ( nMinor > 5 )
		{
			*ppszReason = "unsupported VTF minor version";
			return false;
		}

		const unsigned int nHeaderSize = SprayU32( p + 12 );
		if ( nHeaderSize < 16 || nHeaderSize > 1024 )
		{
			*ppszReason = "bad header size";
			return false;
		}

		const int w = SprayU16( p + 16 );
		const int h = SprayU16( p + 18 );
		if ( w < 1 || h < 1 || w > 8192 || h > 8192 )
		{
			*ppszReason = "bad dimensions";
			return false;
		}

		const unsigned int nFlags = SprayU32( p + 20 );
		if ( nFlags & SPRAY_BAD_FLAGS )
		{
			*ppszReason = "forbidden VTF flags";
			return false;
		}

		const int nFrames = SprayU16( p + 24 );
		if ( nFrames < 1 || nFrames > 66 )
		{
			*ppszReason = "bad frame count";
			return false;
		}

		if ( p[56] < 1 || p[56] > 16 )
		{
			*ppszReason = "bad mip count";
			return false;
		}

		// 7.3+ resource entries append data after the image chain, which the size
		// model below does not account for.
		if ( nMinor >= 3 && SprayU32( p + 68 ) > 0 )
			return true;

		double flExpected = 0.0;
		if ( !SprayComputeTotalSize( p, n, flExpected ) )
			return true;	// unknown format: the header checks are all we can do

		const unsigned int nFileSize = filesystem->Size( pszPath, "GAME" );
		if ( nFileSize >= 1000 && flExpected >= 1000.0 )
		{
			const double flDiff = ( flExpected > (double)nFileSize ) ?
				( flExpected - (double)nFileSize ) : ( (double)nFileSize - flExpected );
			if ( flDiff > flExpected * 0.05 )
			{
				*ppszReason = "image data does not match the file size";
				return false;
			}
		}

		return true;
	}

	void SprayQuarantine( const char *pszPath, const char *pszReason )
	{
		char szBad[MAX_PATH];
		Q_snprintf( szBad, sizeof( szBad ), "%s.bad", pszPath );

		if ( filesystem->FileExists( szBad, "GAME" ) )
			filesystem->RemoveFile( szBad, "GAME" );

		filesystem->RenameFile( pszPath, szBad, "GAME" );

		if ( filesystem->FileExists( pszPath, "GAME" ) )
			Warning( "[SprayCheck] could not quarantine %s (%s)\n", pszPath, pszReason );
		else
			Msg( "[SprayCheck] quarantined %s (%s)\n", pszPath, pszReason );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Budgeted scan of downloads/. Directory listings are cheap and done
//			inline; file reads are capped per frame so a big tree cannot hitch.
//-----------------------------------------------------------------------------
class CSprayCheckSystem : public CAutoGameSystemPerFrame
{
public:
	CSprayCheckSystem() : CAutoGameSystemPerFrame( "SprayCheck" ) { Reset( true ); }

	virtual bool Init() { return true; }
	virtual void LevelInitPostEntity() { SprayCheck_RequestScan(); }
	virtual void LevelShutdownPostEntity() { Reset( true ); }
	virtual void FrameUpdatePreEntityThink() { ServiceScan(); }

	void RequestScan( void );
	bool BlockDecals( void ) const { return m_bFound || m_bScanning; }
	void Reset( bool bClearBlock );

private:
	void ServiceScan( void );
	void ListDirectory( const char *pszDir );
	static int PathDepth( const char *pszDir );

	CUtlVector< CUtlString > m_Dirs;
	CUtlVector< CUtlString > m_Files;
	int m_iDir;
	int m_iFile;
	int m_iChecked;
	int m_iQuarantined;
	bool m_bScanning;
	bool m_bFound;
};

static CSprayCheckSystem g_SprayCheckSystem;

IGameSystem *SprayCheckSystem( void )
{
	return &g_SprayCheckSystem;
}

void SprayCheck_RequestScan( void )
{
	g_SprayCheckSystem.RequestScan();
}

bool SprayCheck_BlockDecals( void )
{
	if ( !sv_spray_enable.GetBool() )
		return true;

	if ( !sv_spray_check.GetBool() )
		return false;

	return g_SprayCheckSystem.BlockDecals();
}

int CSprayCheckSystem::PathDepth( const char *pszDir )
{
	int nDepth = 0;
	for ( const char *p = pszDir; *p; p++ )
	{
		if ( *p == '/' )
			nDepth++;
	}
	return nDepth;
}

void CSprayCheckSystem::Reset( bool bClearBlock )
{
	m_Dirs.RemoveAll();
	m_Files.RemoveAll();
	m_iDir = 0;
	m_iFile = 0;
	m_iChecked = 0;
	m_iQuarantined = 0;
	m_bScanning = false;

	if ( bClearBlock )
		m_bFound = false;
}

void CSprayCheckSystem::RequestScan( void )
{
	if ( !sv_spray_check.GetBool() )
		return;

	Reset( false );

	m_bScanning = true;
	m_Dirs.AddToTail( CUtlString( "downloads" ) );
}

void CSprayCheckSystem::ListDirectory( const char *pszDir )
{
	char szWild[MAX_PATH];
	Q_snprintf( szWild, sizeof( szWild ), "%s/*", pszDir );

	FileFindHandle_t hFind = FILESYSTEM_INVALID_FIND_HANDLE;
	const char *pszName = filesystem->FindFirstEx( szWild, "GAME", &hFind );
	if ( !pszName )
		return;

	const int nDepth = PathDepth( pszDir );

	while ( pszName )
	{
		char szRel[MAX_PATH];
		Q_snprintf( szRel, sizeof( szRel ), "%s/%s", pszDir, pszName );

		if ( filesystem->FindIsDirectory( hFind ) )
		{
			if ( nDepth < SPRAY_SCAN_MAX_DEPTH )
				m_Dirs.AddToTail( CUtlString( szRel ) );
		}
		else
		{
			const int nLen = Q_strlen( pszName );
			if ( nLen > 4 && !Q_stricmp( pszName + nLen - 4, ".dat" ) )
				m_Files.AddToTail( CUtlString( szRel ) );
		}

		pszName = filesystem->FindNext( hFind );
	}

	filesystem->FindClose( hFind );
}

void CSprayCheckSystem::ServiceScan( void )
{
	if ( !m_bScanning )
		return;

	if ( !sv_spray_check.GetBool() )
	{
		m_bScanning = false;
		return;
	}

	int nDone = 0;
	while ( nDone < SPRAY_SCAN_FILES_PER_FRAME )
	{
		if ( m_iFile < m_Files.Count() )
		{
			const char *pszPath = m_Files[m_iFile++].Get();
			nDone++;
			m_iChecked++;

			const char *pszReason = "invalid";
			if ( !SprayFileIsValid( pszPath, &pszReason ) )
			{
				m_bFound = true;
				m_iQuarantined++;
				SprayQuarantine( pszPath, pszReason );
			}
			continue;
		}

		if ( m_iDir < m_Dirs.Count() )
		{
			CUtlString dir = m_Dirs[m_iDir++];
			ListDirectory( dir.Get() );
			continue;
		}

		m_bScanning = false;
		Msg( "[SprayCheck] %d spray file(s) checked, %d quarantined; decals %s\n",
			m_iChecked, m_iQuarantined,
			m_bFound ? "blocked until the next map" : "allowed" );
		break;
	}
}
