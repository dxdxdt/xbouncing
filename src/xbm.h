#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdio.h>


struct xbm {
	unsigned int width;
	unsigned int height;
	size_t bits_size;
	size_t bits_len;
	char *bits;
};

typedef struct xbm xbm_t;

void init_xbm (xbm_t *xbm);
void free_xbm (xbm_t *xbm);
bool alloc_xbm (xbm_t *xbm, const size_t size);
bool load_xbm (
	FILE *f,
	xbm_t *xbm,
	unsigned int *width,
	unsigned int *height);
