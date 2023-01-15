#include <vector>
#include <iostream>
#include <stdlib.h>

int main() {
  std::vector<int> v;
  v.resize(200);

  char* buffer = (char *) malloc(100);
  buffer = (char *) malloc(400);
  (void) buffer;
}

