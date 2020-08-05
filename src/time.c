#pragma once
#include <time.h>
#include <sys/time.h>
#include "type.h"

u64 now() {
  struct timeval st;
  gettimeofday(&st, NULL);
  return st.tv_sec * 1000000ll + st.tv_usec;
}
