/*
 * Copyright 2022 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include "basework/bitops.h"

unsigned long find_first_bit(unsigned long *addr, unsigned long size) {
	long mask;
	int bit;

	for (bit = 0; size >= BITS_PER_LONG;
	    size -= BITS_PER_LONG, bit += BITS_PER_LONG, addr++) {
		if (*addr == 0)
			continue;
		return (bit + __ffsl(*addr));
	}
	if (size) {
		mask = (*addr) & BITMAP_LAST_WORD_MASK(size);
		if (mask)
			bit += __ffsl(mask);
		else
			bit += size;
	}
	
	return bit;
}

unsigned long find_first_zero_bit(unsigned long *addr, unsigned long size) {
	long mask;
	int bit;

	for (bit = 0; size >= BITS_PER_LONG;
	    size -= BITS_PER_LONG, bit += BITS_PER_LONG, addr++) {
		if (~(*addr) == 0)
			continue;
		return (bit + __ffsl(~(*addr)));
	}
	if (size) {
		mask = ~(*addr) & BITMAP_LAST_WORD_MASK(size);
		if (mask)
			bit += __ffsl(mask);
		else
			bit += size;
	}
	
	return bit;
}

unsigned long find_last_bit(unsigned long *addr, unsigned long size) {
	long mask;
	int offs;
	int bit;
	int pos;

	pos = size / BITS_PER_LONG;
	offs = size % BITS_PER_LONG;
	bit = BITS_PER_LONG * pos;
	addr += pos;
	if (offs) {
		mask = (*addr) & BITMAP_LAST_WORD_MASK(offs);
		if (mask)
			return (bit + __flsl(mask));
	} else {
		mask = 0;
	}

	while (--pos) {
		addr--;
		bit -= BITS_PER_LONG;
		if (*addr)
			return (bit + __flsl(mask));
	}
	
	return size;
}

unsigned long find_next_bit(unsigned long *addr, unsigned long size, 
	unsigned long offset) {
	long mask;
	int offs;
	int bit;
	int pos;

	if (offset >= size)
		return (size);
	pos = offset / BITS_PER_LONG;
	offs = offset % BITS_PER_LONG;
	bit = BITS_PER_LONG * pos;
	addr += pos;
	if (offs) {
		mask = (*addr) & ~BITMAP_LAST_WORD_MASK(offs);
		if (mask)
			return (bit + __ffsl(mask));
		if (size - bit <= BITS_PER_LONG)
			return (size);
		bit += BITS_PER_LONG;
		addr++;
	}
	for (size -= bit; size >= BITS_PER_LONG;
	    size -= BITS_PER_LONG, bit += BITS_PER_LONG, addr++) {
		if (*addr == 0)
			continue;
		return (bit + __ffsl(*addr));
	}
	if (size) {
		mask = (*addr) & BITMAP_LAST_WORD_MASK(size);
		if (mask)
			bit += __ffsl(mask);
		else
			bit += size;
	}
	
	return bit;
}

unsigned long find_next_zero_bit(unsigned long *addr, unsigned long size,
    unsigned long offset) {
	long mask;
	int offs;
	int bit;
	int pos;

	if (offset >= size)
		return (size);
	pos = offset / BITS_PER_LONG;
	offs = offset % BITS_PER_LONG;
	bit = BITS_PER_LONG * pos;
	addr += pos;
	if (offs) {
		mask = ~(*addr) & ~BITMAP_LAST_WORD_MASK(offs);
		if (mask)
			return (bit + __ffsl(mask));
		if (size - bit <= BITS_PER_LONG)
			return (size);
		bit += BITS_PER_LONG;
		addr++;
	}
	for (size -= bit; size >= BITS_PER_LONG;
	    size -= BITS_PER_LONG, bit += BITS_PER_LONG, addr++) {
		if (~(*addr) == 0)
			continue;
		return (bit + __ffsl(~(*addr)));
	}
	if (size) {
		mask = ~(*addr) & BITMAP_LAST_WORD_MASK(size);
		if (mask)
			bit += __ffsl(mask);
		else
			bit += size;
	}
	return bit;
}
