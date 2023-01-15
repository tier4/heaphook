#include <vector>
#include <iostream>
#include <stdlib.h>

int main() {
  {
    std::vector<int> v;
    v.resize(200);
  }

  std::vector<char> v2;
  v2.resize(100);

  char* buffer = (char *) malloc(100);
  buffer = (char *) malloc(400);
  (void) buffer;

  int *value = new int(0);
  delete value;

  int *values = new int[10];
  delete[] values;
}

