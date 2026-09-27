#pragma once

#include <Core/DetectionPipeline.hpp>
#include <Core/Logger.hpp>
#include <Core/String.hpp>
#include <Core/Types.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#if defined(PIPELINE_OS_LINUX) && PIPELINE_OS_LINUX
#include <unistd.h>
#elif defined(PIPELINE_OS_WINDOWS) && PIPELINE_OS_WINDOWS
#include <direct.h>
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BuildEnvironment

struct BuildEnvironment
{
  String ProjectRoot;
  String SourceDir;
  String OutputDir;
  String Standard;
  String Compiler;
  String Ninja;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Boot

class Boot
{
public:
    STATIC bool run(BuildEnvironment &env)
	{
      env.SourceDir = "Sources";
      env.OutputDir = "Build";
      env.Standard = "c++20";

      if (!findProjectRoot(env.ProjectRoot, env.SourceDir))
	  {
        LOG_FATAL(
            "Nao encontrei a raiz do projeto (nenhum diretorio '%s' neste "
            "caminho ou nos pais).",
            env.SourceDir.cStr());
        return false;
      }

      loadConfig(env);

      String sourcePath = joinPath(env.ProjectRoot, env.SourceDir);
      if (!pathExists(sourcePath.cStr())) {
        LOG_FATAL("Diretorio de fontes nao encontrado em '%s'.",
                  sourcePath.cStr());
        return false;
      }

      if (!detectCompiler(env.Compiler)) {
        LOG_FATAL("Nenhum compilador C++ encontrado no PATH (tentei $CXX, "
                  "clang++, g++).");
        return false;
      }

      if (!detectTool("ninja", env.Ninja)) {
        LOG_FATAL("'ninja' nao encontrado no PATH.");
        return false;
      }

      LOG_INFO("Project root : %s", env.ProjectRoot.cStr());
      LOG_INFO("Source dir   : %s", env.SourceDir.cStr());
      LOG_INFO("Output dir   : %s", env.OutputDir.cStr());
      LOG_INFO("Compiler     : %s", env.Compiler.cStr());
      LOG_INFO("Ninja        : %s", env.Ninja.cStr());

      return true;
  }

private:
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Path helpers (sempre normaliza para '/')

  STATIC bool isPathSeparator(char c) { return c == '/' || c == '\\'; }

  STATIC String joinPath(const String &a, const StringView &b) {
    if (a.isEmpty())
      return String(b);
    if (b.isEmpty())
      return a;

    char last = a[a.size() - 1];
    if (isPathSeparator(last)) {
      return a + b;
    }

    return a + StringView("/") + b;
  }

  STATIC String normalizePath(const String &path) {
    String result = path;
    for (u64 i = 0; i < result.size(); ++i) {
      if (result[i] == '\\') {
        result[i] = '/';
      }
    }
    return result;
  }

  STATIC bool pathExists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
  }

  STATIC bool getCurrentDirectory(char *buffer, u64 size) {
#if defined(PIPELINE_OS_LINUX) && PIPELINE_OS_LINUX
    return getcwd(buffer, size) != nullptr;
#elif defined(PIPELINE_OS_WINDOWS) && PIPELINE_OS_WINDOWS
    return _getcwd(buffer, static_cast<int>(size)) != nullptr;
#else
    (void)buffer;
    (void)size;
    return false;
#endif
  }

  STATIC bool findProjectRoot(String &outRoot, const String &sourceDirName) {
    char cwd[1024];
    if (!getCurrentDirectory(cwd, sizeof(cwd))) {
      return false;
    }

    String dir = normalizePath(String(cwd));

    for (u64 depth = 0; depth < 32; ++depth) {
      String candidate = joinPath(dir, sourceDirName);

      if (pathExists(candidate.cStr())) {
        outRoot = dir;
        return true;
      }

      // Sobe um nivel (aceita '/' e '\')
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

  STATIC void loadConfig(BuildEnvironment &env) {
    String configPath = joinPath(env.ProjectRoot, StringView("builder.config"));

    FILE *file = fopen(configPath.cStr(), "r");
    if (file == nullptr) {
      return; // Sem config -> ficam os defaults.
    }

    char line[512];
    while (fgets(line, sizeof(line), file) != nullptr) {
      StringView view(line);
      u64 eq = view.find('=');

      if (eq == U64_MAX) {
        continue;
      }

      StringView key = view.subStr(0, eq);
      StringView value = view.subStr(eq + 1);

      // Remove \r\n
      while (!value.isEmpty() && (value[value.size() - 1] == '\n' ||
                                  value[value.size() - 1] == '\r')) {
        value = value.subStr(0, value.size() - 1);
      }

      if (key == StringView("SOURCE_DIR"))
        env.SourceDir = String(value);
      else if (key == StringView("OUTPUT_DIR"))
        env.OutputDir = String(value);
      else if (key == StringView("STANDARD"))
        env.Standard = String(value);
    }

    fclose(file);

    LOG_INFO("Config carregada de '%s'.", configPath.cStr());
  }

  STATIC bool commandExists(const char *name) {
#if defined(PIPELINE_OS_LINUX) && PIPELINE_OS_LINUX
    String command = String("command -v ") + StringView(name) +
                     StringView(" >/dev/null 2>&1");
#elif defined(PIPELINE_OS_WINDOWS) && PIPELINE_OS_WINDOWS
    String command =
        String("where ") + StringView(name) + StringView(" >NUL 2>&1");
#else
    (void)name;
    return false;
#endif
    return system(command.cStr()) == 0;
  }

  STATIC bool detectCompiler(String &outCompiler) {
    const char *envCxx = getenv("CXX");
    if (envCxx != nullptr && envCxx[0] != '\0' && commandExists(envCxx)) {
      outCompiler = String(envCxx);
      return true;
    }

    if (commandExists("clang++")) {
      outCompiler = String("clang++");
      return true;
    }

    if (commandExists("g++")) {
      outCompiler = String("g++");
      return true;
    }

    return false;
  }

  STATIC bool detectTool(const char *name, String &outPath) {
    if (!commandExists(name)) {
      return false;
    }

    outPath = String(name);
    return true;
  }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////