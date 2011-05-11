
#ifndef GC_THREADS
# define GC_THREADS
#endif

#define GC_NO_THREAD_REDIRECTS 1

#include "gc.h"

#include <pthread.h>

pthread_key_t key;
pthread_once_t key_once = PTHREAD_ONCE_INIT;

void * entry (void *arg)
{
  pthread_setspecific(key,
                      (void *)GC_HIDE_POINTER(GC_STRDUP("hello, world")));
  return arg;
}

void * GC_CALLBACK on_thread_exit_inner (struct GC_stack_base * sb, void * arg)
{
  int res = GC_register_my_thread (sb);
  pthread_t t;

  GC_pthread_create (&t, NULL, entry, NULL);
  if (res == GC_SUCCESS)
    GC_unregister_my_thread ();
  return NULL;
}

void on_thread_exit (void *v)
{
  GC_call_with_stack_base (on_thread_exit_inner, NULL);
}

void make_key (void)
{
  pthread_key_create (&key, on_thread_exit);
}

#ifndef LIMIT
# define LIMIT 50
#endif

int main (void)
{
  int i;
  GC_INIT ();

  pthread_once (&key_once, make_key);

  for (i = 0; i < LIMIT; i++)
    {
      pthread_t t;
      GC_pthread_create (&t, NULL, entry, NULL);
    }

  return 0;
}
