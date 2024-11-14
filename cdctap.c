/*
 * Copyright 2024 Andrew B. Hastings. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/*
 * Read CDC I-format tapes in SIMH tape image format.
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <alloca.h>
#include "ansi.h"
#include "dcode.h"
#include "ifmt.h"
#include "outfile.h"
#include "opl.h"
#include "pfdump.h"
#include "rectype.h"
#include "simtap.h"
#include "cdctap.h"


int debug = 0;
int verbose = 0;

static int ascii = 0;
static int lfmt = 0;


/*
 * -d: show structure of PFDUMP record.
 */

int do_dopt(TAPE *tap, int argc, char **argv)
{
	int ec = 0;
	ssize_t nbytes;
	char *tbuf, *cbuf;
	rectype_t rt;
	int i, nchar, ui;
	char name[8], date[11], extra[EXTRA_LEN+1];
	char lbuf[81];
	char *found;
	cdc_ctx_t cd;

	found = alloca(argc);
	if (!found) {
		fprintf(stderr, "too many file names\n");
		return 2;
	}
	memset(found, 0, argc);

	while (1) {
		nbytes = tap_readblock(tap, &tbuf);
		if (nbytes < 0) {
			if (nbytes == -2)
				ec = 2;
			break;
		}

		/* skip tape marks and tape labels */
		if (nbytes == 0)
			continue;
		if (is_label(tbuf, nbytes, lbuf))
			continue;

		/* unpack to 6-bit characters and identify */
		nchar = cdc_ctx_init(&cd, tap, tbuf, nbytes, &cbuf);
		if (nchar == -2) {
			ec = 2;
			break;
		}
		rt = id_record(cbuf, nchar, name, date, extra, &ui);

		/* check for match */
		for (i = 0; i < argc; i++)
			if (name_match(argv[i], name, ui))
				break;
		if (i == argc) {
			/* no match */
			(void) cdc_skipr(&cd);
			cdc_ctx_fini(&cd);
			continue;
		}
		found[i] = 1;

		dprint(("do_dopt: nbytes %ld nchar %d\n", nbytes, nchar));
		switch (rt) {
		    case RT_PFDUMP:
			analyze_pfdump(&cd);
			break;

		    default:
			fprintf(stderr, "Not dumping %s/%s\n",
					rectype[rt], name);
			(void) cdc_skipr(&cd);
		}
		cdc_ctx_fini(&cd);
	}

	for (i = 0; i < argc; i++)
		if (!found[i]) {
			fprintf(stderr, "%s not found\n", argv[i]);
			ec = 3;
		}
	return ec;
}


/*
 * -r: show raw tape block structure.
 */

int do_ropt(TAPE *tap)
{
	ssize_t nbytes;
	int nchar, rv, ec = 0;
	char lbuf[81];
	char *tbuf, *cbuf = NULL;

	while (1) {
		nbytes = tap_readblock(tap, &tbuf);
		if (nbytes < 0) {
			if (nbytes == -2)
				ec = 2;
			break;
		}
		if (nbytes == 0) {
			printf("  --mark--\n");
			continue;
		}
		printf("%5ld ", nbytes);
		if (is_label(tbuf, nbytes, lbuf))
			print_label(lbuf);
		else {
			nchar = nbytes*8/6;
			cbuf = realloc(cbuf, nchar);
			if (!cbuf) {
				fprintf(stderr, "do_ropt: block size %ld "
                                        "(CDC chars %d) too large\n",
					nbytes, nchar);
				ec = 2;
				break;
			}
			rv = unpack6(cbuf, tbuf, nbytes);
			if (rv != nchar) {
				fprintf(stderr, "do_ropt: expect %d, got %d\n",
					nchar, rv);
				ec = 2;
				break;
			}
			print_data(cbuf, nchar);
		}
	}
	free(cbuf);
	return ec;
}


/*
 * -t: catalog the tape.
 */

int do_topt(TAPE *tap)
{
	ssize_t nbytes;
	char *tbuf, *cbuf;
	int ec = 0;
	cdc_ctx_t cd;
	int nchar, ui, in_ulib = 0;
	char name[8], date[11], extra[EXTRA_LEN+1];
	char lbuf[81];
	rectype_t rt;
	int i, reclen;

	i = 0;
	while (1) {
		nbytes = tap_readblock(tap, &tbuf);
		if (nbytes < 0) {
			if (nbytes == -2)
				ec = 2;
			break;
		}
		if (nbytes == 0) {
			printf("  --mark--\n");
			continue;
		}

		if (is_label(tbuf, nbytes, lbuf)) {
			switch (lbuf[0]) {
			    case 'V':
				print_lfield("Catalog of ", lbuf+4, lbuf+9);
				if (print_lfield(" (", lbuf+37, lbuf+50))
					putchar(')');
				break;

			    case 'H':
				print_lfield("\nCatalog of ", lbuf+4, lbuf+20);
				print_jdate(" ", lbuf+41);
				putchar('\n');
				break;

			    default:
				/* ignore other labels */
				break;
			}
			continue;
		}

		/* unpack to 6-bit characters and identify */
		nchar = cdc_ctx_init(&cd, tap, tbuf, nbytes, &cbuf);
		if (nchar == -2) {
			ec = 2;
			break;
		}
		rt = id_record(cbuf, nchar, name, date, extra, &ui);
		reclen = cdc_skipr(&cd);

		/* ULIB: omit contents unless -l */
		if (!lfmt) {
			/* skip until OPLD */
			if (in_ulib) {
				if (rt == RT_OPLD)
					in_ulib = 0;
				cdc_ctx_fini(&cd);
				continue;
			}

			if (rt == RT_ULIB)
				in_ulib = 1;
		}

		/* print record info */
		if (verbose) {
			char *dp = date;

			/* omit trailing space/period, leading space */
			for (nchar = 9; nchar > 7; nchar--)
				if (dp[nchar] == ' ' || dp[nchar] == '.')
					dp[nchar] = '\0';
			if (dp[0] == ' ')
				dp++;

			printf("%-7s %-6s", name, rectype[rt]);
			if (rt > RT_EOF)
				printf(" %7d %8s", reclen, dp);
			if (verbose < 2)
				extra[48] = '\0';
			printf(" %s\n", extra);
		} else {
			switch (rt) {
			    case RT_EOF:
				i = 4;
				/* fall through */
			    case RT_EMPTY:
				printf("%8s%6s", rectype[rt], "");
				break;

			    default:
				printf("%6s/%-7s", rectype[rt], name);
			}
			if (++i > 4) {
				putchar('\n');
				i = 0;
			} else
				putchar(' ');
		}
		cdc_ctx_fini(&cd);
	}
	return ec;
}


/*
 * -x: extract files from tape.
 */

char *extract_text(cdc_ctx_t *cd, char *name)
{
	FILE *of;
	char fname[16];
	int i, oc, eol = 0, esc = 0;
	char c, *cp;

	of = out_open(name, "txt", fname);
	if (!of) {
		(void) cdc_skipr(cd);
		return "";
	}

	while (cp = cdc_getword(cd)) {
		oc = 10;
		while (oc-- && !cp[oc])
			;
		oc++;
		if (eol && oc)
			putc(dcmap[0], of);
		eol = (oc == 9);
		for (i = 0; i < oc; i++) {
			c = cp[i];
			if (ascii && (c == 074 || c == 076)) {
				esc = c;
				continue;
			}
			switch (esc) {
			    case 074:
				fputs(c74map[c], of);
				break;

			    case 076:
				fputs(c76map[c], of);
				break;

			    default:
				putc(dcmap[c], of);
			}
			esc = 0;
		}
		if (oc < 9) {
			if (esc)
				putc(dcmap[esc], of);
			esc = 0;
			putc('\n', of);
		}
	}
	if (esc)
		putc(dcmap[esc], of);
	if (eol)
		putc(dcmap[0], of);

	out_close(of);
	return NULL;
}


int do_xopt(TAPE *tap, int argc, char **argv)
{
	int ec = 0;
	ssize_t nbytes;
	cdc_ctx_t cd;
	char *tbuf, *cbuf;
	int i, nchar, ui;
	char *found;
	rectype_t rt;
	char name[8], date[11], extra[EXTRA_LEN+1];
	char lbuf[81];
	char *fn, *err;

	found = alloca(argc);
	if (!found) {
		fprintf(stderr, "too many file names\n");
		return 3;
	}
	memset(found, 0, argc);

	while (1) {
		nbytes = tap_readblock(tap, &tbuf);
		if (nbytes < 0) {
			if (nbytes == -2)
				ec = 2;
			break;
		}

		/* skip tape marks and tape labels */
		if (nbytes == 0)
			continue;
		if (is_label(tbuf, nbytes, lbuf))
			continue;

		/* unpack to 6-bit characters and identify */
		nchar = cdc_ctx_init(&cd, tap, tbuf, nbytes, &cbuf);
		if (nchar == -2) {
			ec = 2;
			break;
		}
		rt = id_record(cbuf, nchar, name, date, extra, &ui);
		if (!name[0])
			strcpy(name, "noname");

		/* check for name match */
		for (i = 0; i < argc; i++)
			if (fn = name_match(argv[i], name, ui))
				break;
		if (i == argc) {
			/* no match */
			(void) cdc_skipr(&cd);
			cdc_ctx_fini(&cd);
			continue;
		}
		found[i] = 1;
		err = NULL;

		dprint(("do_xopt: nbytes %ld nchar %d\n", nbytes, nchar));
		switch (rt) {
		    case RT_TEXT:
		    case RT_PROC:
			err = extract_text(&cd, fn);
			break;

		    case RT_OPL:
		    case RT_OPLC:
			err = extract_opl(&cd, fn);
			break;

		    case RT_UPL:
			err = extract_upl(&cd, fn);
			break;

		    case RT_UPLR:
			err = extract_uplr(&cd, fn);
			break;

		    case RT_DUMPPF:
			err = extract_dumppf(&cd, fn);
			break;

		    case RT_PFDUMP:
			err = extract_pfdump(&cd, fn);
			break;

		    default:
			if (rt > RT_EOF)
				err = "not extracting";
			(void) cdc_skipr(&cd);
		}

		if (err) {
			ec = 2;
			if (err[0])
				fprintf(stderr, "%s/%s: %s\n", rectype[rt],
					name, err);
		}

		cdc_ctx_fini(&cd);
	}

	for (i = 0; i < argc; i++)
		if (!found[i]) {
			fprintf(stderr, "%s not found\n", argv[i]);
			ec = 2;
		}
	return ec;
}


/*
 * Main program.
 */

char *prog;


void usage(int ec)
{
	fprintf(stderr, "Usage: %s [-3aOv] -f path.tap [-r | -t | -d files... | -x files...]\n",
		prog);
	fprintf(stderr, " -f   file in SIMH tape format (required)\n");
	fprintf(stderr, "operations:\n");
	fprintf(stderr, " -d   show structure of PFDUMP record\n");
	fprintf(stderr, " -r   show raw tape block structure\n");
	fprintf(stderr, " -t   catalog the tape\n");
	fprintf(stderr, " -x   extract files from tape\n");
	fprintf(stderr, "modifiers:\n");
	fprintf(stderr, " -3   use 63-character set (default 64)\n");
	fprintf(stderr, " -a   extract in ASCII mode (6/12 display code)\n");
	fprintf(stderr, " -l   list contents of user libraries\n");
	fprintf(stderr, " -O   extract to stdout (default write to file)\n");
	fprintf(stderr, " -v   verbose output\n");
	fprintf(stderr, " -vv  more verbose output\n");
	exit(ec);
}


#define OP_R	1
#define OP_T	2
#define OP_X	4
#define OP_D	8

void main(int argc, char **argv)
{
	int c, ec;
	unsigned op = 0;
	char *ifile = NULL;
	TAPE *tap;

	prog = strrchr(argv[0], '/');
	prog = prog ? prog+1 : argv[0];

	while ((c = getopt(argc, argv, "3aDdf:hlOrtvx")) != -1) {
		switch (c) {
		    case '3':
			dcmap[063] = ':';
			c74map[04] = "%";
			break;

		    case 'a':
			ascii++;
			break;

		    case 'D':
			debug++;
			break;

		    case 'd':
			op |= OP_D;
			break;

		    case 'f':
			ifile = optarg;
			break;

		    case 'h':
			usage(0);
			break;

		    case 'l':
			lfmt++;
			break;

		    case 'O':
			sout++;
			break;

		    case 'r':
			op |= OP_R;
			break;

		    case 't':
			op |= OP_T;
			break;

		    case 'v':
			verbose++;
			break;

		    case 'x':
			op |= OP_X;
			break;

		    case ':':
			fprintf(stderr, "option -%c requires an operand\n",
				optopt);
			usage(1);
			break;

		    case '?':
			fprintf(stderr, "unrecognized option -%c\n", optopt);
			usage(1);
			break;
		}
	}

	if (!ifile) {
		fprintf(stderr, "-f must be specified\n");
		usage(1);
	}

	switch (op) {
	    case OP_R:
	    case OP_T:
		if (optind < argc) {
			fprintf(stderr, "files not allowed with -%c\n",
				op == OP_R ? 'r' : 't');
			usage(1);
		}
		break;

	    case OP_D:
	    case OP_X:
		if (optind >= argc) {
			fprintf(stderr, "no files specified\n");
			usage(1);
		}
		break;

	    default:
		fprintf(stderr,
			"must specify exactly one of -d, -r, -t, or -x\n");
		usage(1);
	}


	if (debug)
		setbuf(stdout, NULL);

	if (!(tap = tap_open(ifile, NULL))) {
		perror(ifile);
		exit(1);
	}

	switch (op) {
	    case OP_D:	ec = do_dopt(tap, argc-optind, argv+optind); break;
	    case OP_R:	ec = do_ropt(tap); break;
	    case OP_T:	ec = do_topt(tap); break;
	    case OP_X:	ec = do_xopt(tap, argc-optind, argv+optind); break;
	}

	tap_close(tap);

	exit(ec);
}
