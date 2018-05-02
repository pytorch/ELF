/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "common.h"
#include <stdarg.h>
#include <stdio.h>

double __attribute__((noinline)) wallclock(void) {
  struct timeval t;
  gettimeofday(&t, NULL);
  return (1.0e-6 * t.tv_usec + t.tv_sec);
}

uint64_t __attribute__((noinline)) wallclock64() {
  return (uint64_t)(wallclock() * 1e6);
}

void dbg_printf(const char* format, ...) {
  (void)(format);
#ifdef DEBUG
  va_list argptr;
  va_start(argptr, format);
  printf("INFO: ");
  vprintf(format, argptr);
  va_end(argptr);
  printf("\n");
  fflush(stdout);
#endif
}

void error(const char* format, ...) {
  va_list argptr;
  va_start(argptr, format);
  printf("ERROR: ");
  vprintf(format, argptr);
  va_end(argptr);
  printf("\n");
  fflush(stdout);
  // Make an easy sev.
  char* a = NULL;
  *a = 1;
  exit(1);
}
