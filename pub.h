#ifndef __PUB_H__
#define __PUB_H__

#define atomic_inc(p)   __sync_add_and_fetch(p, 1)
#define atomic_dec(p)   __sync_sub_and_fetch(p, 1)
#define atomic_read(p)  __sync_fetch_and_add(p, 0)

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

#endif

