#include "Build.hpp"

#include <Core/Platform/FileSystem.hpp>

#include <string.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Parser

STATIC bool containsMain( const char *filePath )
{
	char *buffer = nullptr;
	u64   read   = FileSystem::readFile( filePath, buffer );

	if ( buffer == nullptr ) return false;

	bool found = false;

	for ( u64 i = 0; i + 8 <= read; ++i )
	{
		if ( memcmp( buffer + i, "int main", 8 ) == 0 )
		{
			found = true;
			break;
		}
	}

	Memory::free( buffer, read + 1 );
	return found;
}

STATIC void collectSources( const String &fullDir, const String &relativeDir, Array<String> &outSources )
{
	Array<DirEntry> entries;
	if ( !FileSystem::listEntries( fullDir, entries ) ) return;

	for ( u64 i = 0; i < entries.size(); ++i )
	{
		const DirEntry &entry = entries[i];

		String childFull     = FileSystem::joinPath( fullDir, entry.Name );
		String childRelative = FileSystem::joinPath( relativeDir, entry.Name );

		if ( entry.Type == EntryType::Directory )
		{
			collectSources( childFull, childRelative, outSources );
		}
		else if ( FileSystem::hasExtension( entry.Name, ".cpp" ) )
		{
			outSources.add( childRelative );
		}
	}
}

STATIC void handleModuleDirectory( const String &name, const String &fullPath, Array<Target> &outTargets )
{
	Target target;
	target.Name = name;

	String relativeDir = FileSystem::joinPath( Env::SourceDir, name );
	collectSources(fullPath, relativeDir, target.Sources);

	if (target.Sources.isEmpty())
	{
		LOG_DEBUG("Modulo '%s' so tem cabecalhos - sem target de compilacao.", name.cStr() );
		return;
	}

	bool hasMain = false;
	for (u64 s = 0; s < target.Sources.size() && !hasMain; ++s) {
		String absolute = FileSystem::joinPath( Env::ProjectRoot, target.Sources[s] );
		hasMain = containsMain(absolute.cStr());
	}

	target.Type = hasMain ? TargetType::Executable : TargetType::Library;

	LOG_INFO("Target %s '%s' (%llu arquivo(s)).",
					 hasMain ? "executavel" : "de biblioteca", target.Name.cStr(),
					 target.Sources.size());

	outTargets.add(static_cast<Target &&>(target));
}

STATIC void handleLooseSource( const String &name, const String &fullPath, Array<Target> &outTargets){
	if (!containsMain(fullPath.cStr())) {
		LOG_WARNING("'%s' esta solto na raiz de Source/ e nao tem main() - "
								"ignorado (deveria estar em um modulo?).",
								name.cStr());
		return;
	}

	Target target;
	target.Name = FileSystem::stripExtension(name);
	target.Type = TargetType::Executable;
	target.Sources.add(FileSystem::joinPath( Env::SourceDir, name ));

	LOG_INFO("Target executavel '%s'.", target.Name.cStr());
	outTargets.add(static_cast<Target &&>(target));
}

bool Build::init_parser( Array<Target> &outTargets )
{
	String sourceRoot = FileSystem::joinPath( Env::ProjectRoot, Env::SourceDir );

	Array<DirEntry> entries;
	if ( !FileSystem::listEntries( sourceRoot, entries ) )
	{
		LOG_FATAL("Nao consegui abrir o diretorio de fontes '%s'.",sourceRoot.cStr());
		return false;
	}

	for ( u64 i = 0; i < entries.size(); ++i )
	{
		const DirEntry &entry = entries[i];
		String fullPath = FileSystem::joinPath( sourceRoot, entry.Name );

		if ( entry.Type == EntryType::Directory ) handleModuleDirectory( entry.Name, fullPath, outTargets );
		else if ( FileSystem::hasExtension( entry.Name, ".cpp" ) ) handleLooseSource( entry.Name, fullPath, outTargets );
	}

	if ( outTargets.isEmpty()) LOG_WARNING("Nenhum target encontrado em '%s'.", sourceRoot.cStr());

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////