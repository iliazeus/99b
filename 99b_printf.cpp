#include <cstdio>
#include <cstdint>
#include <cstdlib>

int main(int argc, const char **argv) {
  uint32_t max_bottle_count = 0xFFFFFF;
  if (argc > 1) max_bottle_count = std::strtoul(argv[1], 0, 10);

  for (uint32_t bottle_count = max_bottle_count; bottle_count > 0; bottle_count -= 1) {
    std::printf("%-10u bottles of beer on the wall,\n"
    "%-10u bottles of beer.\n"
    "Take one down, pass it around\n"
    "%-10u bottles of beer on the wall.\n\n",
    bottle_count, bottle_count, bottle_count - 1);
  }
}
