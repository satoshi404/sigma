#pragma once

#include <Core/Types.hpp>
#include <Core/String.hpp>
#include <Core/Array.hpp>
#include <Core/Memory.hpp>
#include <Core/DetectionPipeline.hpp>

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

#if defined( PIPELINE_OS_LINUX ) && PIPELINE_OS_LINUX
	#include <unistd.h>
#elif defined( PIPELINE_OS_WINDOWS ) && PIPELINE_OS_WINDOWS
	#include <direct.h>
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// EntryType

enum EntryType : u8
{
	File,
	Directory,
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DirEntry

struct DirEntry
{
	String    Name;
	EntryType Type;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSystem

class FileSystem
{
public:

	STATIC bool isPathSeparator( char c )
	{
		return c == '/' || c == '\\';
	}

	STATIC String joinPath( const String& a, const StringView& b )
	{
		if ( a.isEmpty() )
		{
			return String( b );
		}

		if ( b.isEmpty() )
		{
			return a;
		}

		char last = a[ a.size() - 1 ];

		if ( isPathSeparator( last ) )
		{
			return a + b;
		}

		return a + StringView( "/" ) + b;
	}

	STATIC String normalizePath( const String& path )
	{
		String result = path;

		for ( u64 i = 0; i < result.size(); ++i )
		{
			if ( result[ i ] == '\\' )
			{
				result[ i ] = '/';
			}
		}

		return result;
	}

	STATIC String parentDirectory( const String& path )
	{
		StringView view( path );
		u64        slash = U64_MAX;

		for ( u64 i = view.size(); i > 0; --i )
		{
			if ( isPathSeparator( view[ i - 1 ] ) )
			{
				slash = i - 1;
				break;
			}
		}

		if ( slash == U64_MAX || slash == 0 )
		{
			return String( "." );
		}

		return String( view.subStr( 0, slash ) );
	}

	STATIC bool isDotEntry( const StringView& name )
	{
		return name == StringView( "." ) || name == StringView( ".." );
	}

	STATIC bool hasExtension( const StringView& name, const char* ext )
	{
		u64 extLen = strlen( ext );

		if ( name.size() < extLen )
		{
			return false;
		}

		return memcmp( name.cStr() + ( name.size() - extLen ), ext, extLen ) == 0;
	}

	STATIC String stripExtension( const StringView& name )
	{
		u64 dot = U64_MAX;

		for ( u64 i = name.size(); i > 0; --i )
		{
			if ( name[ i - 1 ] == '.' )
			{
				dot = i - 1;
				break;
			}
		}

		return ( dot == U64_MAX ) ? String( name ) : String( name.subStr( 0, dot ) );
	}

	STATIC bool pathExists( const char* path )
	{
		struct stat st;
		return stat( path, &st ) == 0;
	}

	STATIC bool isDirectory( const char* path )
	{
		struct stat st;

		if ( stat( path, &st ) != 0 )
		{
			return false;
		}

		return S_ISDIR( st.st_mode );
	}

	STATIC bool getCurrentDirectory( char* buffer, u64 size )
	{
	#if defined( PIPELINE_OS_LINUX ) && PIPELINE_OS_LINUX
		return getcwd( buffer, size ) != nullptr;
	#elif defined( PIPELINE_OS_WINDOWS ) && PIPELINE_OS_WINDOWS
		return _getcwd( buffer, static_cast<int>( size ) ) != nullptr;
	#else
		(void)buffer; (void)size;
		return false;
	#endif
	}

	STATIC bool ensureDirectory( const char* path )
	{
		struct stat st;

		if ( stat( path, &st ) == 0 )
		{
			return true;
		}

		String parent = parentDirectory( String( path ) );

		if ( parent != String( "." ) && parent != String( path ) )
		{
			if ( !ensureDirectory( parent.cStr() ) )
			{
				return false;
			}
		}

	#if defined( PIPELINE_OS_LINUX ) && PIPELINE_OS_LINUX
		return mkdir( path, 0755 ) == 0;
	#elif defined( PIPELINE_OS_WINDOWS ) && PIPELINE_OS_WINDOWS
		return _mkdir( path ) == 0;
	#else
		return false;
	#endif
	}

	STATIC bool listEntries( const String& directory, Array<DirEntry>& outEntries )
	{
		DIR* dir = opendir( directory.cStr() );

		if ( dir == nullptr )
		{
			return false;
		}

		struct dirent* entry;

		while ( ( entry = readdir( dir ) ) != nullptr )
		{
			String name( entry->d_name );

			if ( isDotEntry( name ) )
			{
				continue;
			}

			String   fullPath = joinPath( directory, name );
			DirEntry item;

			item.Type = isDirectory( fullPath.cStr() ) ? EntryType::Directory : EntryType::File;
			item.Name = static_cast<String&&>( name );

			outEntries.add( static_cast<DirEntry&&>( item ) );
		}

		closedir( dir );
		return true;
	}

	STATIC u64 readFile( const char* path, char*& outBuffer )
	{
		FILE* file = fopen( path, "rb" );

		if ( file == nullptr )
		{
			outBuffer = nullptr;
			return 0;
		}

		fseek( file, 0, SEEK_END );
		long fileSize = ftell( file );
		fseek( file, 0, SEEK_SET );

		if ( fileSize <= 0 )
		{
			fclose( file );
			outBuffer = nullptr;
			return 0;
		}

		outBuffer = static_cast<char*>( Memory::alloc( static_cast<u64>( fileSize ) + 1 ) );
		u64 read  = fread( outBuffer, 1, static_cast<u64>( fileSize ), file );
		outBuffer[ read ] = '\0';

		fclose( file );
		return read;
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////