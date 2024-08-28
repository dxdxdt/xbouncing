#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "xbm.h"

static void do_help (FILE *os, const char *program_name) {
	fprintf(os,
"Load an XBM file from STDIN, print the bits back to the STDOUT\n"
"Usage: %s [-h]\n"
"Options:\n"
"  -h  print this message and exit normally\n"
	, program_name);
}

static void do_opts (const int argc, const char **argv) {
	int o;

	while (true) {
		o = getopt(argc, (char *const *)argv, "h");
		if (o < 0) {
			break;
		}

		switch (o) {
		case 'h':
			do_help(stdout, argv[0]);
			exit(0);
			break; // compiler, stfu
		default:
			do_help(stderr, argv[0]);
			exit(2);
		}
	}
}

int main (const int argc, const char **argv) {
	int ec = 0;
	unsigned int c;
	xbm_t xbm;
	size_t p;

	do_opts(argc, argv);

	init_xbm(&xbm);

	if (!load_xbm(stdin, &xbm, NULL, NULL)) {
		ec = 1;
		perror("load_xbm()");
		goto END;
	}

	for (size_t i = 0; i < xbm.bits_len; i += 1) {
		c = (unsigned char)xbm.bits[i];
		printf("%02x%c", c, (i + 1) % 24 == 0 ? '\n' : ' ');
	}

	if (p % 24 != 0) {
		printf("\n");
	}

END:
	free_xbm(&xbm);

	return ec;
}
