#ifndef __efl_hash_h__
#define __efl_hash_h__

typedef unsigned long   hash_t;

inline hash_t efl_hash ( const wchar_t * s )
{
	const wchar_t *name = (const wchar_t*) (wchar_t*) s;
	unsigned long h = 0, g;

	while (*name != L'\0') { /* do some fancy bitwanking on the string */
		h = (h << 4) + (hash_t)(*name++);
		if ((g = (h & 0xF0000000UL))!=0)
			h ^= (g >> 24);
		h &= ~g;
	}

	return h;
}

#endif