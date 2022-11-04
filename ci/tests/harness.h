/** A simple harness for the C benchmarks
 *
 * It is included by every benchmark file and provides the time measuring,
 * as well as standardized command-line argument parsing.
 *
 *  Released under Public domain.
 *  Author:  S. Marr  University of Kent
 */

#include <sys/time.h>

unsigned long microseconds() {
  // Not monotonic
  struct timeval t;
  gettimeofday(&t, NULL);
  return (t.tv_sec * 1000 * 1000) + t.tv_usec;
};

void parse_argv(int argc,
                char** argv,
                int* iterations,
                int* warmup,
                int* inner_or_problem_size) {
  if (argc > 1) {
    *iterations = atoi(argv[1]);
  }
  if (argc > 2) {
    *warmup     = atoi(argv[2]);
  }
  if (argc > 3) {
    *inner_or_problem_size = atoi(argv[3]);
  }

  printf("Overall iterations:     %d\n", *iterations);
  printf("Warmup  iterations:     %d\n", *warmup);
  printf("Inner it./problem size: %d\n", *inner_or_problem_size);
};
