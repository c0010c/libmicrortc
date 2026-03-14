#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdio.h>
#include <stdlib.h>

#define ASSERT_TRUE(expr)                                                                    \
  do {                                                                                       \
    if (!(expr)) {                                                                           \
      fprintf(stderr, "ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr);   \
      exit(1);                                                                               \
    }                                                                                        \
  } while (0)

#define ASSERT_EQ_INT(a, b)                                                                  \
  do {                                                                                       \
    int _a = (int)(a);                                                                       \
    int _b = (int)(b);                                                                       \
    if (_a != _b) {                                                                          \
      fprintf(stderr, "ASSERT_EQ_INT failed at %s:%d: %d != %d\n", __FILE__, __LINE__, _a, _b); \
      exit(1);                                                                               \
    }                                                                                        \
  } while (0)

#endif
