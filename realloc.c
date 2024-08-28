/*
  Simple shared object to replace realloc and check for misbehavior
when realloc changes the base pointer.
It is not uncommon to have applications that crash randomly, and
the root cause is a realloc call changing the base pointer.

Sample usage:

$ gcc -fPIC -shared realloc.c -o realloc.so -ldl -pthread -D_GNU_SOURCE=1
$ LD_PRELOAD=$PWD/realloc.so /path/to/binary

  To be more complete it would be required to somehow make sure the old
ptr is watched somehow in a debugger, or have the memory unmapped to
cause a SEGV.
 */

#include <malloc.h>
#include <string.h>
#include <dlfcn.h>
#include <execinfo.h>

/* Keep last pointer around to prevent it being returned by another
 * memory allocation too fast. */
__thread void *keep;

void *
realloc(void *ptr, size_t size)
{
    void	*new_ptr;
    size_t	 old_size;
    static void *(*real_realloc)(void*, size_t) = (void*)0;
    if (!real_realloc)
	real_realloc = dlsym(RTLD_NEXT, "realloc");
    old_size = malloc_usable_size(ptr);
    /* If cannot force pointer change just call the real_realloc. */
    if ((new_ptr = malloc(size)) == (void*)0)
	return real_realloc(ptr, size);
    (void)memcpy(new_ptr, ptr, old_size > size ? size : old_size);
    /* Attempt to speed up a segfault if some code keeps a pointer
     * to the old pointer.
     * Could use mallopt(M_PERTURB, 0x5a) or set the MALLOC_PERTURB_
     * environment variable.
     */
    (void)memset(ptr, 0x5a, old_size);
    free(keep);
    keep = ptr;
    return new_ptr;
}
