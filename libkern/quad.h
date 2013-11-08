#ifndef __QUAD_H__
#define __QUAD_H__

/* eduOS prelude */
#include <eduos/stddef.h>
#include <asm/limits.h>

typedef uint64_t	u_quad_t;
typedef int64_t		quad_t;
typedef uint32_t	u_long;
typedef uint32_t 	qshift_t;

/*
 * Depending on the desired operation, we view a `long long' (aka quad_t) in
 * one or more of the following formats.
 */
union uu {
	quad_t		q;		/* as a (signed) quad */
	quad_t		uq;		/* as an unsigned quad */
	int32_t		sl[2];		/* as two signed longs */
	uint32_t	ul[2];		/* as two unsigned longs */
};

#define CHAR_BIT	8

#if BYTE_ORDER == LITTLE_ENDIAN
#define _QUAD_HIGHWORD	1
#define _QUAD_LOWWORD	0
#else
#define _QUAD_HIGHWORD	0
#define _QUAD_LOWWORD	1
#endif

/*
 * Define high and low longwords.
 */
#define H		_QUAD_HIGHWORD
#define L		_QUAD_LOWWORD

/*
 * Total number of bits in a quad_t and in the pieces that make it up.
 * These are used for shifting, and also below for halfword extraction
 * and assembly.
 */
#define QUAD_BITS	(sizeof(quad_t) * CHAR_BIT)
#define LONG_BITS	(sizeof(long) * CHAR_BIT)
#define HALF_BITS	(sizeof(long) * CHAR_BIT / 2)

u_quad_t __qdivrem(u_quad_t uq, u_quad_t vq, u_quad_t* arq);

#endif
