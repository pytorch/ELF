/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifndef _COMMON_H_
#define _COMMON_H_

#include <inttypes.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include "../Game.h"
#ifdef __cplusplus
extern "C" {
#endif

double __attribute__((noinline)) wallclock(void);
uint64_t __attribute__((noinline)) wallclock64();

void dbg_printf(const char* format, ...);
void error(const char* format, ...);

#ifdef __cplusplus
}
#endif

#define __STR_EXPAND(tok) #tok
#define __STR(tok) __STR_EXPAND(tok)

typedef unsigned short Coord;
typedef unsigned char Stone;

#define S_EMPTY 0
#define S_BLACK 1
#define S_WHITE 2
#define S_OFF_BOARD 3

// Two special moves.
#define M_PASS 0 // (-1, -1)
#define M_RESIGN 1 // (0, -1)
// Used when we want to skip and let the opponent play.
#define M_SKIP 2
#define M_INVALID 3
#define M_CLEAR 4

#define STR_BOOL(s) ((s) ? "true" : "false")
#define STR_STONE(s) ((s) == S_BLACK ? "B" : ((s) == S_WHITE ? "W" : "U"))

#define timeit \
  {            \
    double __start = wallclock();

#define endtime                              \
  double __duration = wallclock() - __start; \
  printf("Time spent = %lf\n", __duration);  \
  }

#define endtime2(t)          \
  t = wallclock() - __start; \
  }

#endif
