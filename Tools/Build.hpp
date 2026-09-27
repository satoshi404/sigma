#pragma once

#include <Core/Array.hpp>
#include <Core/DetectionPipeline.hpp>
#include <Core/Logger.hpp>
#include <Core/String.hpp>
#include <Core/Types.hpp>

#include "Parser.hpp"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#if defined(PIPELINE_OS_WINDOWS) && PIPELINE_OS_WINDOWS
#include <direct.h>
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Build

class Build {
public:
  STATIC bool run(const BuildEnvironment &env, const Array<Target> &targets) {
    String outputPath = joinPath(env.ProjectRoot, env.OutputDir);

    if (!ensureDirectory(outputPath.cStr())) {
      LOG_FATAL("Nao consegui criar o diretorio de saida '%s'.",
                outputPath.cStr());
      return false;
    }

    // Pastas que o Ninja vai precisar (sem isso ele falha ao escrever .o / exe)
    ensureDirectory(joinPath(outputPath, StringView("bin")).cStr());
    ensureDirectory(joinPath(outputPath, StringView("lib")).cStr());
    ensureDirectory(joinPath(outputPath, StringView("obj")).cStr());

    if (!writeNinjaFile(env, outputPath, targets)) {
      return false;
    }

    if (!writeCompileCommands(env, outputPath, targets)) {
      return false;
    }

    // TODO: Removed
    //if (!writeClangdConfig(env)) {
      //return false;
    //}

   // LOG_INFO(
     //   "Gerados '%s/build.ninja', '%s/compile_commands.json' e '.clangd'.",
       // env.OutputDir.cStr(), env.OutputDir.cStr());
    return true;
  }

private:
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Path helpers

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

  STATIC String escapeNinjaPath(const String &path) {
    // Conta quantos ':' existem para saber o tamanho final
    u64 colonCount = 0;
    for (u64 i = 0; i < path.size(); ++i) {
      if (path[i] == ':') {
        colonCount += 1;
      }
    }

    u64 finalLen = path.size() + colonCount; // cada ':' vira "$:"
    char *buffer = static_cast<char *>(malloc(finalLen + 1));

    u64 out = 0;
    for (u64 i = 0; i < path.size(); ++i) {
      if (path[i] == ':') {
        buffer[out++] = '$';
        buffer[out++] = ':';
      } else {
        buffer[out++] = path[i];
      }
    }
    buffer[out] = '\0';

    String result(buffer); // usa o construtor público
    free(buffer);

    return result;
  }
  STATIC String parentDirectory(const String &path) {
    StringView view(path);
    u64 slash = U64_MAX;

    for (u64 i = view.size(); i > 0; --i) {
      if (isPathSeparator(view[i - 1])) {
        slash = i - 1;
        break;
      }
    }

    if (slash == U64_MAX || slash == 0) {
      return String(".");
    }

    return String(view.subStr(0, slash));
  }

  STATIC bool ensureDirectory(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
      return true;
    }

    // Cria pais primeiro (recursivo simples)
    String parent = parentDirectory(String(path));
    if (parent != String(".") && parent != String(path)) {
      if (!ensureDirectory(parent.cStr())) {
        return false;
      }
    }

#if defined(PIPELINE_OS_LINUX) && PIPELINE_OS_LINUX
    return mkdir(path, 0755) == 0;
#elif defined(PIPELINE_OS_WINDOWS) && PIPELINE_OS_WINDOWS
    return _mkdir(path) == 0;
#else
    return false;
#endif
  }

  STATIC String objectPathFor(const BuildEnvironment &env,
                              const String &source)
  {
    StringView relative(source);

    if (source.size() > env.SourceDir.size() &&
        memcmp(source.cStr(), env.SourceDir.cStr(), env.SourceDir.size()) ==
            0 &&
        (source[env.SourceDir.size()] == '/' ||
         source[env.SourceDir.size()] == '\\')) {
      relative = relative.subStr(env.SourceDir.size() + 1);
    }

    return String("obj/") + relative + StringView(".o");
  }

  STATIC bool writeNinjaFile(const BuildEnvironment &env,
                             const String &outputPath,
                             const Array<Target> &targets) {
    String path = joinPath(outputPath, StringView("build.ninja"));

    FILE *file = fopen(path.cStr(), "w");
    if (file == nullptr) {
      LOG_FATAL("Nao consegui escrever '%s'.", path.cStr());
      return false;
    }

    String includeRoot = joinPath(env.ProjectRoot, env.SourceDir);
    String escapedInclude = escapeNinjaPath(includeRoot);

    fprintf(
        file,
        "# Gerado automaticamente por tools/builder - nao edite a mao.\n\n");
    fprintf(file, "cxx      = %s\n", env.Compiler.cStr());
    fprintf(file, "cxxflags = -std=%s -Wall -Wextra -I%s -MMD -MF $out.d\n\n",
            env.Standard.cStr(), escapedInclude.cStr());

    fprintf(file, "rule cxx\n");
    fprintf(file, "  command = $cxx $cxxflags -c $in -o $out\n");
    fprintf(file, "  depfile = $out.d\n");
    fprintf(file, "  deps = gcc\n");
    fprintf(file, "  description = CXX $out\n\n");

    fprintf(file, "rule link_exe\n");
    fprintf(file, "  command = $cxx $in -o $out\n");
    fprintf(file, "  description = LINK $out\n\n");

    fprintf(file, "rule archive\n");
    fprintf(file, "  command = ar rcs $out $in\n");
    fprintf(file, "  description = AR $out\n\n");

    Array<Array<String>> objectsPerTarget;

    for (u64 t = 0; t < targets.size(); ++t) {
      const Target &target = targets[t];
      Array<String> objects;

      for (u64 s = 0; s < target.Sources.size(); ++s) {
        String obj = objectPathFor(env, target.Sources[s]); // relativo
        String source =
            joinPath(env.ProjectRoot, target.Sources[s]); // absoluto

        ensureDirectory(joinPath(outputPath, parentDirectory(obj)).cStr());

        String escapedSource = escapeNinjaPath(source);

        fprintf(file, "build %s: cxx %s\n", obj.cStr(), escapedSource.cStr());
        objects.add(static_cast<String &&>(obj));
      }

      objectsPerTarget.add(static_cast<Array<String> &&>(objects));
    }

    fprintf(file, "\n");

    Array<String> defaultTargets;

    for (u64 t = 0; t < targets.size(); ++t) {
      const Target &target = targets[t];

      if (target.Type == TargetType::Executable) {
        String artifact = String("bin/") + target.Name;
#if defined(PIPELINE_OS_WINDOWS) && PIPELINE_OS_WINDOWS
        artifact = artifact + StringView(".exe");
#endif

        fprintf(file, "build %s: link_exe", artifact.cStr());

        for (u64 o = 0; o < objectsPerTarget[t].size(); ++o) {
          fprintf(file, " %s", objectsPerTarget[t][o].cStr());
        }

        // Linka também todos os .o das bibliotecas
        for (u64 other = 0; other < targets.size(); ++other) {
          if (other == t || targets[other].Type != TargetType::Library)
            continue;

          for (u64 o = 0; o < objectsPerTarget[other].size(); ++o) {
            fprintf(file, " %s", objectsPerTarget[other][o].cStr());
          }
        }

        fprintf(file, "\n\n");
        defaultTargets.add(static_cast<String &&>(artifact));
      } else // Library
      {
        String artifact = String("lib/lib") + target.Name + StringView(".a");

        fprintf(file, "build %s: archive", artifact.cStr());

        for (u64 o = 0; o < objectsPerTarget[t].size(); ++o) {
          fprintf(file, " %s", objectsPerTarget[t][o].cStr());
        }

        fprintf(file, "\n\n");
      }
    }

    if (!defaultTargets.isEmpty()) {
      fprintf(file, "default");
      for (u64 i = 0; i < defaultTargets.size(); ++i) {
        fprintf(file, " %s", defaultTargets[i].cStr());
      }
      fprintf(file, "\n");
    }

    fclose(file);
    return true;
  }
  STATIC bool writeCompileCommands(const BuildEnvironment &env,
                                   const String &outputPath,
                                   const Array<Target> &targets) {
    (void)outputPath;

    String path = joinPath(env.ProjectRoot, env.OutputDir);
    path = joinPath(path, StringView("compile_commands.json"));

    FILE *file = fopen(path.cStr(), "w");
    if (file == nullptr) {
      LOG_FATAL("Nao consegui escrever '%s'.", path.cStr());
      return false;
    }

    fprintf(file, "[\n");

    bool first = true;
    String includeRoot = joinPath(env.ProjectRoot, env.SourceDir);

    for (u64 t = 0; t < targets.size(); ++t) {
      const Target &target = targets[t];

      for (u64 s = 0; s < target.Sources.size(); ++s) {
        if (!first) {
          fprintf(file, ",\n");
        }
        first = false;

        String absoluteSource = joinPath(env.ProjectRoot, target.Sources[s]);

        fprintf(file, "  {\n");
        fprintf(file, "    \"directory\": \"%s\",\n", env.ProjectRoot.cStr());
        fprintf(file, "    \"file\": \"%s\",\n", absoluteSource.cStr());
        fprintf(file,
                "    \"command\": \"%s -std=%s -Wall -Wextra -I%s -c %s\"\n",
                env.Compiler.cStr(), env.Standard.cStr(), includeRoot.cStr(),
                absoluteSource.cStr());
        fprintf(file, "  }");
      }
    }

    fprintf(file, "\n]\n");
    fclose(file);
    return true;
  }

  // TODO: Removed
  // STATIC bool writeClangdConfig(const BuildEnvironment &env) {
  //   String path = joinPath(env.ProjectRoot, StringView(".clangd"));

  //   FILE *file = fopen(path.cStr(), "w");
  //   if (file == nullptr) {
  //     LOG_FATAL("Nao consegui escrever '%s'.", path.cStr());
  //     return false;
  //   }

  //   String includeRoot = joinPath(env.ProjectRoot, env.SourceDir);

  //   fprintf(file, "CompileFlags:\n");
  //   fprintf(file, "  Add:\n");
  //   fprintf(file, "    - -std=%s\n", env.Standard.cStr());
  //   fprintf(file, "    - -Wall\n");
  //   fprintf(file, "    - -Wextra\n");
  //   fprintf(file, "    - -I%s\n", includeRoot.cStr());
  //   fprintf(file, "  CompilationDatabase: %s\n", env.OutputDir.cStr());

  //   fclose(file);
  //   return true;
  // }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////