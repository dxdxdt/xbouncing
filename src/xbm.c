#include "xbm.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>


void init_xbm (xbm_t *xbm) {
	memset(xbm, 0, sizeof(xbm_t));
}

void free_xbm (xbm_t *xbm) {
	free(xbm->bits);
	memset(xbm, 0, sizeof(xbm_t));
}

bool alloc_xbm (xbm_t *xbm, const size_t size) {
	bool ret = false;

	if (size > 0) {
		void *ptr = realloc(xbm->bits, size);

		if (ptr != NULL) {
			xbm->bits = ptr;
			xbm->bits_size = size;
			ret = true;
		}
	}
	else {
		free_xbm(xbm);
		ret = true;
	}

	return ret;
}

static ssize_t first_nonwhitespace (const char *s, const size_t *n) {
	size_t i = 0;
	ssize_t ret = -1;

	while (true) {
		if (s[i] == 0 || (n != NULL && *n <= i)) {
			break;
		}
		if (!isspace(s[i])) {
			ret = i;
			break;
		}

		i += 1;
	}

	return ret;
}

static ssize_t first_whitespace (const char *s, const size_t *n) {
	size_t i = 0;
	ssize_t ret = -1;

	while (true) {
		if (s[i] == 0 || (n != NULL && *n <= i)) {
			break;
		}
		if (isspace(s[i])) {
			ret = i;
			break;
		}

		i += 1;
	}

	return ret;
}

static bool ends_with (const char *haystack, const char *needle) {
	char *n = strstr(haystack, needle);
	return n != NULL && haystack + (strlen(haystack) - strlen(needle)) == n;
}

#define LINEBUF_SIZE (4096)

static bool read_dim (FILE *f, unsigned int *width, unsigned int *height) {
	static const size_t LBS = LINEBUF_SIZE;
	char linebuf[LINEBUF_SIZE], *l;
	ssize_t fc;
	bool has[2] = { false, };
	bool *target_has;
	unsigned int *target_d;

	while (fgets(linebuf, LINEBUF_SIZE, f) != NULL) {
		fc = first_nonwhitespace(linebuf, &LBS);
		if (fc < 0) {
			continue;
		}
		l = linebuf + fc;

		if (strstr(l, "#define") != l) {
			continue;
		}
		l += sizeof("#define");
		fc = first_nonwhitespace(l, NULL);
		if (fc < 0) {
			continue;
		}
		l += fc;

		fc = first_whitespace(l, NULL);
		if (fc < 0) {
			continue;
		}
		l[fc] = 0;

		if (ends_with(l, "_width")) {
			target_has = has + 0;
			target_d = width;
		}
		else if (ends_with(l, "_height")) {
			target_has = has + 1;
			target_d = height;
		}
		else {
			continue;
		}

		l += fc + 1;
		*target_has = sscanf(l, "%u", target_d) > 0;
		if (*target_has && *target_d == 0) {
			break;
		}

		if (has[0] && has[1]) {
			return true;
		}
	}

	errno = EBADMSG;
	return false;
}

static bool str_atleast (const char *s, const size_t l) {
	for (size_t i = 0; i < l; i += 1) {
		if (s[i] == 0) {
			return false;
		}
	}

	return true;
}

static bool read_bits (FILE *f, xbm_t *xbm, size_t *count) {
	char linebuf[LINEBUF_SIZE], *l;
	unsigned int c;
	size_t p = 0;

	while (fgets(linebuf, LINEBUF_SIZE, f) != NULL) {
		l = linebuf;

		while (true) {
			l = strstr(l, "0x");
			if (l == NULL) {
				break;
			}
			else if (!str_atleast(l, sizeof("0x00") - 1)) {
				continue;
			}
			l[sizeof("0x00") - 1] = 0;

			if (sscanf(l + 2, "%x", &c) != 1) {
				errno = EBADMSG;
				return false;
			}

			if (xbm != NULL) {
				if (xbm->bits_size <= p &&
						!alloc_xbm(xbm, xbm->bits_size + 4096))
				{
					return false;
				}
				xbm->bits[p] = (char)c;
			}

			p += 1;
			l += sizeof("0x00");
		}
	}

	if (count != NULL) {
		*count = p;
	}
	if (xbm != NULL) {
		xbm->bits_len = p;
	}

	return true;
}

#undef LINEBUF_SIZE

bool load_xbm (
		FILE *f,
		xbm_t *xbm,
		unsigned int *out_width,
		unsigned int *out_height)
{
	unsigned int width, height;
	bool ret = false;
	xbm_t out;

	init_xbm(&out);

	if (!read_dim(f, &width, &height)) {
		goto END;
	}

	ret = read_bits(f, &out, NULL);
	if (ret) {
		if (xbm != NULL) {
			free_xbm(xbm);
			*xbm = out;
			xbm->width = width;
			xbm->height = height;
			init_xbm(&out);
		}

		if (out_width) {
			*out_width = width;
		}
		if (out_height) {
			*out_height = height;
		}
	}

END:
	free_xbm(&out);
	return ret;
}
