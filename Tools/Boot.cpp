#include "Build.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

namespace Env
{
	String SourceDir 	= "Sources";
	String OutputDir 	= "Build";
	String Standard 	= "c++20";
	String ProjectRoot  = {0};
	String Compiler 	= {0};
	String Ninja 		= {0};
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Boot

STATIC bool isPathSeparator( char x ) { return x == '/' || x == '\\'; }

STATIC String joinPath( const String &a, const StringView &b )
{
	if ( a.isEmpty() ) return String( b );
	if ( b.isEmpty() ) return a;

	char last = a[a.size() - 1];
	if ( isPathSeparator( last ) ) return a + b;

	return a + StringView( "/") + b;
}

STATIC String normalizePath(const String &path)
{
	String result = path;
	for ( u64 i = 0; i < result.size(); ++i )
	if (result[i] == '\\') result[i] = '/';;

	return result;
}

STATIC bool pathExists( const char *path )
{
	struct stat st;
	return stat( path, &st ) == 0;
}

STATIC bool getCurrentDirectory( char *buffer, u64 size )
{
	bool exist = false;

	#if PIPELINE_OS_LINUX
		exist = getcwd( buffer, size ) != nullptr;
	#elif PIPELINE_OS_WINDOWS
		exist = _getcwd( buffer, static_cast<int>( size ) ) != nullptr;
	#endif

	return exist;
}

STATIC bool findProjectRoot()
{
	char cwd[1024];
	if (!getCurrentDirectory(cwd, sizeof(cwd))) return false;

	String dir = normalizePath( String( cwd ) );

	for ( u64 depth = 0; depth < 32; ++depth )
	{
		String candidate = joinPath(dir, Env::SourceDir);

		LOG_DEBUG( "candidate: %s" ,candidate.cStr() );

		if ( pathExists( candidate.cStr() ) )
		{
			Env::ProjectRoot = dir;
			return true;
		}

		StringView view(dir);
		u64 slash = U64_MAX;

		for (u64 i = view.size(); i > 0; --i) {
			if (isPathSeparator(view[i - 1])) {
				slash = i - 1;
				break;
			}
		}

		if (slash == U64_MAX || slash == 0) {
			break;
		}

		dir = String(view.subStr(0, slash));
	}

	return false;
}

STATIC bool loadConfig()
{
	String configPath = joinPath( Env::ProjectRoot, StringView( "builder.config" ) );

	FILE *file = fopen( configPath.cStr(), "r" );
	if ( file == nullptr ) return true; // Sem config -> ficam os defaults, isso nao e erro.

	char line[512];
	while ( fgets( line, sizeof( line ), file ) != nullptr )
	{
		StringView view( line );
		u64 eq = view.find( '=' );

		if ( eq == U64_MAX ) continue;

		StringView key = view.subStr( 0, eq );
		StringView value = view.subStr( eq + 1 );

		while (!value.isEmpty() && (	 value[value.size() - 1] == '\n'
									||   value[value.size() - 1] == '\r' ) )
			value = value.subStr(0, value.size() - 1);


		if ( key == StringView( "SOURCE_DIR" ) )      Env::SourceDir = String( value );
		else if ( key == StringView( "OUTPUT_DIR" ) ) Env::OutputDir = String( value );
		else if ( key == StringView( "STANDARD" ) )   Env::Standard  = String( value );
		else LOG_WARNING( "Unknown key" );
	}

	fclose( file );
	file = nullptr;

	LOG_INFO( "Config carregada de '%s'.", configPath.cStr() );

	return true;
}

STATIC bool commandExists( const char *name )
{
	#if PIPELINE_OS_LINUX
		String command = String("command -v ") + StringView(name) + StringView(" >/dev/null 2>&1");
	#elif PIPELINE_OS_WINDOWS
		String command = String("where ") + StringView(name) + StringView(" >NUL 2>&1");
	#else
		(void)name;
		return false;
	#endif

	return system(command.cStr()) == 0;
}

STATIC bool detectCompiler()
{
	const char *envCxx = getenv( "CXX" );

	if ( envCxx != nullptr && envCxx[0] != '\0' && commandExists( envCxx ) )
	{
		Env::Compiler = String( envCxx );
		return true;
	}

	if ( commandExists( "clang++" ) )
	{
		Env::Compiler = String("clang++");
		return true;
	}

	if (commandExists("g++")) {
		Env::Compiler = String("g++");
		return true;
	}

	return false;
}

STATIC bool detectTool( const char *name, String &outPath )
{
	if ( !commandExists( name ) ) return false;


	outPath = String( name );
	return true;
}

bool Build::init_boot()
{
	if ( !findProjectRoot() )
	{
		LOG_FATAL( "Not found project root '%s'", Env::SourceDir.cStr());
    	return false; }

  	if ( !loadConfig() ) return false;

  	const String sourcePath = joinPath( Env::ProjectRoot, Env::SourceDir );
  	if ( !pathExists( sourcePath.cStr() ) )
	{
   		LOG_FATAL("Diretorio de fontes nao encontrado em '%s'.", sourcePath.cStr());
    	return false;
  	}

  	if ( !detectCompiler())
	{
    	LOG_FATAL("Nenhum compilador C++ encontrado no PATH (tentei $CXX, clang++, g++).");
    	return false;
  	}

  	if ( !detectTool("ninja", Env::Ninja))
	{
    	LOG_FATAL("'ninja' nao encontrado no PATH.");
    	return false;
  	}

  	LOG_INFO( "Project root : %s", Env::ProjectRoot.cStr() );
  	LOG_INFO( "Source dir   : %s", Env::SourceDir.cStr() );
  	LOG_INFO( "Output dir   : %s", Env::OutputDir.cStr() );
  	LOG_INFO( "Compiler     : %s", Env::Compiler.cStr() );
  	LOG_INFO( "Ninja        : %s", Env::Ninja.cStr() );

  return true;
}