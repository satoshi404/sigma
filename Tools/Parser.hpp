#pragma once

#include <Core/Types.hpp>
#include <Core/String.hpp>
#include <Core/Array.hpp>
#include <Core/Logger.hpp>

#include "Boot.hpp"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Target

enum TargetType : u8
{
	Library,
	Executable,
};

struct Target
{
	String        Name;
	TargetType    Type;
	Array<String> Sources;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Parser

class Parser
{
public:

	STATIC bool run( const BuildEnvironment& env, Array<Target>& outTargets )
	{
		String sourceRoot = env.ProjectRoot + StringView( "/" ) + env.SourceDir;

		DIR* dir = opendir( sourceRoot.cStr() );
		if ( dir == nullptr )
		{
			LOG_FATAL( "Nao consegui abrir o diretorio de fontes '%s'.", sourceRoot.cStr() );
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

			String fullPath = sourceRoot + StringView( "/" ) + name;

			if ( isDirectory( fullPath.cStr() ) )
			{
				handleModuleDirectory( env, name, fullPath, outTargets );
			}
			else if ( hasExtension( name, ".cpp" ) )
			{
				handleLooseSource( env, name, fullPath, outTargets );
			}
		}

		closedir( dir );

		if ( outTargets.isEmpty() )
		{
			LOG_WARNING( "Nenhum target encontrado em '%s'.", sourceRoot.cStr() );
		}

		return true;
	}

private:

	STATIC void handleModuleDirectory( const BuildEnvironment& env, const String& name, const String& fullPath, Array<Target>& outTargets )
	{
		Target target;
		target.Name = name;

		String relativeDir = env.SourceDir + StringView( "/" ) + name;
		collectSources( fullPath, relativeDir, target.Sources );

		if ( target.Sources.isEmpty() )
		{
			LOG_DEBUG( "Modulo '%s' so tem cabecalhos - sem target de compilacao.", name.cStr() );
			return;
		}

		bool hasMain = false;
		for ( u64 s = 0; s < target.Sources.size() && !hasMain; ++s )
		{
			String absolute = env.ProjectRoot + StringView( "/" ) + target.Sources[ s ];
			hasMain = containsMain( absolute.cStr() );
		}

		target.Type = hasMain ? TargetType::Executable : TargetType::Library;

		LOG_INFO( "Target %s '%s' (%llu arquivo(s)).",
			hasMain ? "executavel" : "de biblioteca", target.Name.cStr(), target.Sources.size() );

		outTargets.add( static_cast<Target&&>( target ) );
	}

	STATIC void handleLooseSource( const BuildEnvironment& env, const String& name, const String& fullPath, Array<Target>& outTargets )
	{
		if ( !containsMain( fullPath.cStr() ) )
		{
			LOG_WARNING( "'%s' esta solto na raiz de Source/ e nao tem main() - ignorado (deveria estar em um modulo?).", name.cStr() );
			return;
		}

		Target target;
		target.Name = stripExtension( name );
		target.Type = TargetType::Executable;
		target.Sources.add( env.SourceDir + StringView( "/" ) + name );

		LOG_INFO( "Target executavel '%s'.", target.Name.cStr() );
		outTargets.add( static_cast<Target&&>( target ) );
	}

	STATIC void collectSources( const String& fullDir, const String& relativeDir, Array<String>& outSources )
	{
		DIR* dir = opendir( fullDir.cStr() );
		if ( dir == nullptr )
		{
			return;
		}

		struct dirent* entry;
		while ( ( entry = readdir( dir ) ) != nullptr )
		{
			String name( entry->d_name );

			if ( isDotEntry( name ) )
			{
				continue;
			}

			String childFull     = fullDir     + StringView( "/" ) + name;
			String childRelative = relativeDir + StringView( "/" ) + name;

			if ( isDirectory( childFull.cStr() ) )
			{
				collectSources( childFull, childRelative, outSources );
			}
			else if ( hasExtension( name, ".cpp" ) )
			{
				outSources.add( childRelative );
			}
		}

		closedir( dir );
	}

	STATIC bool isDotEntry( const StringView& name )
	{
		return name == StringView( "." ) || name == StringView( ".." );
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

	STATIC bool containsMain( const char* filePath )
	{
		FILE* file = fopen( filePath, "rb" );
		if ( file == nullptr )
		{
			return false;
		}

		fseek( file, 0, SEEK_END );
		long fileSize = ftell( file );
		fseek( file, 0, SEEK_SET );

		if ( fileSize <= 0 )
		{
			fclose( file );
			return false;
		}

		char* buffer = static_cast<char*>( Memory::alloc( static_cast<u64>( fileSize ) + 1 ) );
		u64   read   = fread( buffer, 1, static_cast<u64>( fileSize ), file );
		fclose( file );

		bool found = false;

		for ( u64 i = 0; i + 8 <= read; ++i )
		{
			if ( memcmp( buffer + i, "int main", 8 ) == 0 )
			{
				found = true;
				break;
			}
		}

		Memory::free( buffer, static_cast<u64>( fileSize ) + 1 );
		return found;
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////