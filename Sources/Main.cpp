#include <Core/Array.hpp>
#include <Core/Logger.hpp>
#include <Core/String.hpp>

int main()
{
  LOG_INFO("Ola Mundo");

  {
    Array<int> list;
    list.add(3);
    list.add(5);
    list.add(3);

    for (const int item : list) {
      LOG_WARNING("Item is: %i", item);
    }
  }

  String msg = "Ola mundo";

  if (msg.isEmpty())
    LOG_FATAL("String empty");

  LOG_DEBUG(msg.cStr());

  LOG_FATAL("Test faltal ( Shutup )");

  return 0;
}