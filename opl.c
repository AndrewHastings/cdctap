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
 * MODIFY and UPDATE PL routines.
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alloca.h>
#include <time.h>
#include "cdctap.h"
#include "dcode.h"
#include "ifmt.h"
#include "opl.h"
#include "outfile.h"
#include "simtap.h"


/* set lastmask = 040 for UPDATE PLs */
/* returns -2 if early EOR, -1 if not found, else mod number */
int read_hist(cdc_ctx_t *cd, char *cp, int idx, int lastmask)
{
	int rv = -1;
	int hist;

	/* iterate through modification history "bytes" (18 bits) */
	while (1) {
		hist = (cp[idx] << 12) | (cp[idx + 1] << 6) | cp[idx + 2];

		/* end of history "bytes"? */
		if (!hist)
			break;

		/* bit 16 = activated the line */
		if (hist & 0200000)
			rv = hist & 0177777;

		idx += 3;
		if (idx > 9) {
			/* last history word? */
			if (cp[0] & lastmask)
				break;

			if (!(cp = cdc_getword(cd)))
				return -2;
			idx = 1;
		}
	}

	return rv;
}


#define MAXLEN		    160	    /* max expanded line length */

#define EXPAND_IS_64	    1
#define EXPAND_63_IS_COL    2	    /* only for MODIFY OPL */

/* return -1=line too long, -2=EOR, else remaining word count */
int expand_text(cdc_ctx_t *cd, int wc, char *obuf, int flags)
{
	int state = 0;		/* 0=default, 1=00, 2=0077, 3=007700 */
	int i;
	char c, *cp, *op;

	op = obuf;
	for (; wc > 0; wc--) {
		if (op - obuf > MAXLEN)	/* line length exceeded? */
			return -1;
		if (!(cp = cdc_getword(cd)))
			return -2;
		dprint(("expand_text: cp=%p wc=%d\n", cp, wc));
		for (i = 0; i < 10; i++) {
			c = cp[i];
			dprint(("expand_text: state=%d c=%d\n", state, c));
			if (c == 0) {
				/* 0000 = end-of-line */
				if (state == 1)
					break;
				/* 0077 -> 007700 transition? */
				if (state == 2) {
					state = 3;
					continue;
				}
				/* 00770000 is invalid; treat as 00 */
				if (state == 3)
					dprint(("expand_text: 00770000\n"));
				/* single 00 */
				state = 1;
				continue;
			}

			/* 0001 expansion depends on OPL charset */
			if (state == 1 && c == 1 && (flags & EXPAND_IS_64)) {
				/* 64-character set ':' */
				*op++ = dcmap[0];
				state = 0;
				continue;
			}

			/* 00xx or 007700xx: expand spaces */
			if (state == 1 || state == 3) {
				int j;

				state = 0;
				if (op - obuf + c > MAXLEN)
					return -1;
				for (j = 0; j < c+1; j++)
					*op++ = ' ';
				if (c == 077)
					state = 2;
				continue;
			}

			/* xx or 0077xx: normal char */
			state = 0;
			/* 063 is always ':' if OPL charset is 63 */
			*op++ = c == 063 && (flags & EXPAND_63_IS_COL)
					? ':'
					: dcmap[c];
		}

		/* EOL marker found: mark word as consumed, exit loop */
		if (i < 10) {
			wc--;
			break;
		}
	}
	*op = '\0';

	return wc;
}


/* MODIFY OPL/OPLC */
char *extract_opl(cdc_ctx_t *cd, char *name)
{
	FILE *of;
	struct tm tm;
	char fname[16], deck[8];
	char *cp, *mods;
	int i, len, nmods, nread;
	int is_ascii = 0, flags = EXPAND_63_IS_COL;
	int width = 72, seqon = 1;

	dprint(("extract_opl: %s\n", name));
	memset(&tm, 0, sizeof tm);
	tm.tm_hour = 12;

	/* process 7700 table */
	cp = cdc_getword(cd);
	if (!cp || cp[0] != 077 || cp[1] != 0)
		return "no 7700 table";
	len = (cp[2] << 6) | cp[3];
	dprint(("extract_opl: 7700 len=%d\n", len));

	if (!(cp = cdc_getword(cd)))
		return "short 7700 table";
	copy_dc(cp, deck, 7, DC_ALNUM);		/* deck name */
	nread = 1;

	if (len >= 3) {
		/* extract creation and modification dates */
		char mdate[11];

		if (!(cp = cdc_getword(cd)))
			return "EOR reading cdate from 7700 table";
		copy_dc(cp, mdate, 10, DC_NONUL);
		if (!(cp = cdc_getword(cd)))
			return "EOR reading mdate from 7700 table";
		if (cp[0])    /* prefer mdate if present, else use cdate */
			copy_dc(cp, mdate, 10, DC_NONUL);
		nread = 3;

		(void) parse_date(mdate, &tm);
	}

	if (len >= 14) {
		/* extract 63/64 charset flag */
		if (!cdc_skipwords(cd, 13 - nread))
			return "EOR reading 7700 table";
		if (!(cp = cdc_getword(cd)))
			return "EOR reading charset from 7700 table";
		if (cp[8] <= 1 && cp[9] == 064)
			flags = EXPAND_IS_64;
		if (cp[8] == 1 && (cp[9] == 0 || cp[9] == 064))
			is_ascii = 1;
		nread = 14;
	}

	dprint(("extract_opl: nread %d ascii %d flags %d\n",
		 nread, is_ascii, flags));
	if (!cdc_skipwords(cd, len - nread))
		return "EOR skipping over 7700 table";

	/* process 7001/7002 table */
	cp = cdc_getword(cd);
	if (!cp || cp[0] != 070 || cp[1] != 1 && cp[1] != 2)
		return "no 700x table";
	nmods = ((cp[8] << 6) | cp[9]) + 1;
	mods = alloca(nmods * 8);
	if (!mods)
		return "too many modsets";
	memset(mods, 0, nmods * 8);
	strcpy(mods, deck);
	for (i = 8; i < nmods * 8; i += 8) {
		if (!(cp = cdc_getword(cd)))
			return "700x table too short";
		copy_dc(cp, mods+i, 7, DC_ALNUM);
		dprint(("extract_opl: mod %s%c\n", mods+i,
			" *"[(cp[7] & 020) >> 4]));  /* bit 16 = yanked */
	}

	of = out_open(name, "txt", fname);
	if (!of) {
		(void) cdc_skipr(cd);
		return "";
	}

	/* iterate through text lines */
	while (cp = cdc_getword(cd)) {
		int active, wc, seq, modnum;
		char *modname = "unknown";
		char obuf[MAXLEN + 12];

		active = cp[0] & 040;
		wc = cp[0] & 037;
		seq = (cp[1] << 12) | (cp[2] << 6) | cp[3];

		/* first history "byte" (18 bits) is bits 35-18 */
		modnum = read_hist(cd, cp, 4, 0);
		if (modnum == -2) {
			out_close(of);
			return "EOR reading modification history";
		}
		if (modnum >= 0)
			modname = modnum < nmods ? mods + modnum*8 : "invalid";

		/* skip over inactive line */
		if (!active) {
			if (!cdc_skipwords(cd, wc))
				break;
			continue;
		}

		dprint(("extract_opl: line %s:%d wc=%d\n", modname, seq, wc));

		/* process compressed text */
		wc = expand_text(cd, wc, obuf, flags);

		if (wc == -2) {
			out_close(of);
			return "EOR reading compressed text";
		}

		if (wc == -1) {
			out_close(of);
			return "line too long in compressed text";
		}

		if (wc) {
			out_close(of);
			return "missing EOL in compressed text";
		}

		/* TBD: *SEQ, *NOSEQ, *WIDTH n */
		if (verbose && seqon || verbose > 1)
			fprintf(of, "%-*.*s%-7s%6d\n", width, width, obuf,
						       modname, seq);
		else
			fprintf(of, "%s\n", obuf);
	}

	out_close(of);
	set_mtime(fname, &tm);
	return NULL;
}


/* sequential UPDATE PL */
char *extract_upl(cdc_ctx_t *cd, char *name, struct tm *tm)
{
	FILE *of;
	char fname[16];
	char *cp;
	char *ids;
	int width = verbose > 1 ? 80 : 72;
	int flags = 0;
	int i, deckcnt, idcnt;

	dprint(("extract_upl: %s\n", name));

	/* process sequential OLDPL header: must start with "CHECK" */
	cp = cdc_getword(cd);
	if (!cp || memcmp(cp, "\003\010\005\003\013", 5) != 0 || (cp[5] & 076))
		return "invalid OLDPL header";
	if (cp[6] != 036)	    /* '3' */
		flags = EXPAND_IS_64;

	if (!(cp = cdc_getword(cd)))
		return "short OLDPL header";
	idcnt = (cp[4] << 12) | (cp[5] << 6) | cp[6];
	deckcnt = (cp[7] << 12) | (cp[8] << 6) | cp[9];
	dprint(("extract_upl: ids %d decks %d\n", idcnt, deckcnt));

	/* process OLDPL directory */
	ids = alloca(idcnt * 10);
	if (!ids)
		return "too many ids";
	memset(ids, 0, idcnt * 10);
	for (i = 0; i < idcnt * 10; i += 10) {
		if (!(cp = cdc_getword(cd)))
			return "OLDPL directory too short";
		copy_dc(cp, ids+i, 9, DC_ALNUM);
		dprint(("extract_upl: mod %s\n", ids+i));
	}

	/* skip over OLDPL deck list */
	if (!cdc_skipwords(cd, deckcnt))
		return "EOR skipping over OLDPL deck list";

	of = out_open(name, "txt", fname);
	if (!of) {
		(void) cdc_skipr(cd);
		return "";
	}

	/* iterate through text lines */
	while (cp = cdc_getword(cd)) {
		int active, wc, seq, modnum;
		char *modname = "unknown";
		char obuf[MAXLEN + 12];

		/* checksum word? */
		if (memcmp(cp, "\000\000\000\000\000", 5) == 0)
			break;

		active = cp[0] & 020;
		wc = (cp[1] << 12) | (cp[2] << 6) | cp[3];
		seq = (cp[4] << 12) | (cp[5] << 6) | cp[6];

		/* first history "byte" (18 bits) is bits 17-0 */
		modnum = read_hist(cd, cp, 7, 040);
		if (modnum == -2) {
			out_close(of);
			return "EOR reading modification history";
		}
		if (modnum > 0)
			modname = modnum <= idcnt ? ids + 10 * (modnum - 1)
						  : "invalid";

		/* skip over inactive line */
		if (!active) {
			if (!cdc_skipwords(cd, wc))
				break;
			continue;
		}

		dprint(("extract_upl: line %s:%d wc=%d\n", modname, seq, wc));

		/* process compressed text */
		wc = expand_text(cd, wc, obuf, flags);

		if (wc == -2) {
			out_close(of);
			return "EOR reading compressed text";
		}

		if (wc == -1) {
			out_close(of);
			return "line too long in compressed text";
		}

		if (wc) {
			out_close(of);
			return "missing EOL in compressed text";
		}

		if (verbose)
			fprintf(of, "%-*.*s%s.%d\n", width, width, obuf,
						     modname, seq);
		else
			fprintf(of, "%s\n", obuf);
	}

	out_close(of);
	set_mtime(fname, tm);
	return NULL;
}


/* random UPDATE PL */
char *extract_uplr(cdc_ctx_t *cd, char *name, struct tm *tm)
{
	FILE *of;
	char fname[16];
	char *cp;
	int flags = (dcmap[063] == ':') ? 0 : EXPAND_IS_64;
	int width = verbose > 1 ? 80 : 72;

	/*
	 * XXX In an UPDATE random PL, the directory of identifiers is found
	 * in a separate record following the decks.
	 *
	 * Currently, extract_uplr() does not have access to that directory,
	 * so the identifier field is displayed as a "d" followed by the
	 * identifier number in octal.
	 */

	dprint(("extract_uplr: %s\n", name));

	of = out_open(name, "txt", fname);
	if (!of) {
		(void) cdc_skipr(cd);
		return "";
	}

	/* iterate through text lines */
	while (cp = cdc_getword(cd)) {
		int active, wc, seq, modnum;
		char obuf[MAXLEN + 12];

		active = cp[0] & 020;
		wc = (cp[1] << 12) | (cp[2] << 6) | cp[3];
		seq = (cp[4] << 12) | (cp[5] << 6) | cp[6];

		/* first history "byte" (18 bits) is bits 17-0 */
		modnum = read_hist(cd, cp, 7, 040);
		if (modnum == -2) {
			out_close(of);
			return "EOR reading modification history";
		}

		/* skip over inactive line */
		if (!active) {
			if (!cdc_skipwords(cd, wc))
				break;
			continue;
		}

		dprint(("extract_uplr: line d%06o:%d wc=%d\n", modnum, seq, wc));

		/* process compressed text */
		wc = expand_text(cd, wc, obuf, flags);

		if (wc == -2) {
			out_close(of);
			return "EOR reading compressed text";
		}

		if (wc == -1) {
			out_close(of);
			return "line too long in compressed text";
		}

		if (wc) {
			out_close(of);
			return "missing EOL in compressed text";
		}

		if (verbose)
			fprintf(of, "%-*.*sd%06o.%d\n", width, width, obuf,
							modnum, seq);
		else
			fprintf(of, "%s\n", obuf);
	}

	out_close(of);
	set_mtime(fname, tm);
	return NULL;
}
