#include "Build.hpp"

int main()
{
  BuildEnvironment env;

  if (!Boot::run(env)) return 1;

  Array<Target> targets;

  if (!Parser::run(env, targets)) return 1;
  if (!Build::run(env, targets)) return 1;

  return 0;
}