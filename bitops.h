/*
 * Copyright 2022 wtcat
 */
#ifndef BASE_BITOPS_H_
#define BASE_BITOPS_H_

#ifdef __cplusplus
extern "C"{
#endif

#undef BIT_MASK
#undef BITS_PER_LONG
#undef GENMASK

#ifndef BIT
#define	BIT(nr)			(1UL << (nr))
#endif

#define UL(nr)  (nr ## UL)
#define ULL(nr) (nr ## ULL)

#define	BITS_PER_LONG	   (sizeof(long) * BITS_PER_BYTE)
#define BITS_PER_LONG_LONG (sizeof(long long) * BITS_PER_BYTE)
#define BITS_PER_BYTE		8

#define BIT_ULL(nr)			(ULL(1) << (nr))
#define BIT_MASK(nr)		(UL(1) << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)		((nr) / BITS_PER_LONG)
#define BIT_ULL_MASK(nr)	(ULL(1) << ((nr) % BITS_PER_LONG_LONG))
#define BIT_ULL_WORD(nr)	((nr) / BITS_PER_LONG_LONG)
#define	BITMAP_FIRST_WORD_MASK(start)	(~UL(0) << ((start) % BITS_PER_LONG))
#define	BITMAP_LAST_WORD_MASK(n)	(~UL(0) >> (BITS_PER_LONG - (n)))

#define	GENMASK(hi, lo)	(((~UL(0)) << (lo)) & (~UL(0) >> (BITS_PER_LONG - 1 - (hi))))

#define __bf_shf(x) (__builtin_ffsll(x) - 1)
	 
/**
 * FIELD_MAX() - produce the maximum value representable by a field
 * @_mask: shifted mask defining the field's length and position
 *
 * FIELD_MAX() returns the maximum value that can be held in the field
 * specified by @_mask.
 */
#define FIELD_MAX(_mask)						\
	({								\
		(typeof(_mask))((_mask) >> __bf_shf(_mask));		\
	})

/**
 * FIELD_FIT() - check if value fits in the field
 * @_mask: shifted mask defining the field's length and position
 * @_val:  value to test against the field
 *
 * Return: true if @_val can fit inside @_mask, false if @_val is too big.
 */
#define FIELD_FIT(_mask, _val)						\
	({								\
		!((((typeof(_mask))_val) << __bf_shf(_mask)) & ~(_mask)); \
	})

/**
 * FIELD_PREP() - prepare a bitfield element
 * @_mask: shifted mask defining the field's length and position
 * @_val:  value to put in the field
 *
 * FIELD_PREP() masks and shifts up the value.  The result should
 * be combined with other fields of the bitfield using logical OR.
 */
#define FIELD_PREP(_mask, _val)						\
	({								\
		((typeof(_mask))(_val) << __bf_shf(_mask)) & (_mask);	\
	})

/**
 * FIELD_GET() - extract a bitfield element
 * @_mask: shifted mask defining the field's length and position
 * @_reg:  value of entire bitfield
 *
 * FIELD_GET() extracts the field specified by @_mask from the
 * bitfield passed in as @_reg by masking and shifting it down.
 */
#define FIELD_GET(_mask, _reg)						\
	({								\
		(typeof(_mask))(((_reg) & (_mask)) >> __bf_shf(_mask));	\
	})


#ifndef ffs
#define	ffs(_x)	  __builtin_ffs((unsigned int)(_x))
#endif /* ffs */

#ifndef ffsl
#define	ffsl(_x)  __builtin_ffsl((unsigned long)(_x))
#endif /* ffsl */

#ifndef ffsll
#define	ffsll(_x) __builtin_ffsll((unsigned long long)(_x))
#endif /* ffsll */

#ifndef fls
#define	fls(_x)	\
     ({ __typeof__(_x) __x = (_x); \
        __x != 0 ? sizeof(__x) * 8 - __builtin_clz((unsigned int)__x) : 0;})
#endif /* fls */

#ifndef flsl
#define	flsl(_x) \
    ({ __typeof__(_x) __x = (_x); \
       __x != 0 ? sizeof(__x) * 8 - __builtin_clzl((unsigned long)__x) : 0;})
#endif /* flsl */

#ifndef flsll
#define	flsll(_x) \
    ({ __typeof__(_x) __x = (_x); \
       __x != 0 ? sizeof(__x) * 8 - __builtin_clzll((unsigned long long)__x) : 0;})
#endif /* flsll */

static inline int __ffs(int mask) {
	return ffs(mask) - 1;
}

static inline int __fls(int mask) {
	return fls(mask) - 1;
}

static inline int __ffsl(long mask) {
	return ffsl(mask) - 1;
}

static inline int __flsl(long mask) {
	return flsl(mask) - 1;
}

#define	ffz(mask)	__ffs(~(mask))

unsigned long find_first_bit(unsigned long *addr, unsigned long size);
unsigned long find_first_zero_bit(unsigned long *addr, unsigned long size);
unsigned long find_last_bit(unsigned long *addr, unsigned long size);
unsigned long find_next_bit(unsigned long *addr, unsigned long size, 
	unsigned long offset);
unsigned long find_next_zero_bit(unsigned long *addr, unsigned long size,
    unsigned long offset);
	
#ifdef __cplusplus
}
#endif
#endif /* BASE_BITOPS_H_ */
