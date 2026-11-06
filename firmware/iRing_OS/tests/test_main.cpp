#include <stdio.h>
extern "C" int run_tests();
int main() {
  int line = run_tests();
  if (line) {
    fprintf(stderr, "Assertion failed at source line %d\n", line);
    return 1;
  }
  return 0;
}
