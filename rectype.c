/*
 * Copyright 2024 Andrew B. Hastings. All rights reserved.
 *
 */

/*
 * Identify CDC record type (adapted from COMCSRT).
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "cdctap.h"
#include "dcode.h"
#include "ifmt.h"
#include "pfdump.h"
#include "rectype.h"


char *rectype[] = {
    "(00)",
    "EOF",
    "TEXT",
    "PROC",
    "7700",
    "OPL",
    "OPLC",
    "OPLD",
    "PP",
    "PPU",
    "PPL",
    "ULIB",
    "REL",
    "ABS",
    "OVL",
    "CAP",
    "PFDUMP",
    "PFDUMP",
};


rectype_t id_record(char *bp, int cnt,
		    char *name, char *date, char *extra, int *ui)
{
	int hdr, len;
	char *np = bp;
	int ncnt = cnt;

	name[0] = date[0] = extra[0] = 0;
	*ui = -1;
	if (cnt < 0)
		return RT_EOF;
	if (cnt == 0)
		return RT_EMPTY;

	/* check for ".PROC," */
	if (strncmp(bp, "\057\020\022\017\003\056", 6) == 0) {
		copy_dc(bp+6, name, MIN(7, cnt-6), DC_ALNUM);
		copy_dc(bp, extra, MIN(EXTRA_LEN, cnt), DC_TEXT);
		return RT_PROC;
	}

	/* check for PFDUMP format */
	if (cnt >= 20) {
		int i, eos = 0;
		int cw = (bp[7] << 12) | (bp[8] << 6) | bp[9];

		/* end of dump marker */
		if (memcmp(bp, "\000\000\000\000\000\000\000\007\070\000",
			   10) == 0 && cnt <= 20)
			return RT_PFLBL;

		/* first 2 words must have matching, valid names */
		for (i = 0; i < 7; i++) {
			if (bp[i] != bp[i+10] ||	/* no match */
			    i > 36            ||	/* non-alphanumeric */
			    eos && bp[i])		/* embedded null */
				break;
			if (!bp[i])			/* null terminator */
				eos = 1;
		}
		dprint(("id_record: cw %06o\n", cw));

		/* label must have "PFDUMP" and proper control word */
		if (memcmp(bp+10, "\020\006\004\025\015\020", 7) == 0 &&
		    cnt >= 80 && cw == 01100 && i >= 6) {
			copy_dc(bp, name, 7, DC_ALNUM);
			copy_dc(bp+40, date, 10, DC_NONUL);

			format_pflabel(extra, bp+10);

			return RT_PFLBL;
		}

		if (i == 7) {
			/* file must have proper control word */
			if ((cw & 0777000) == 011000 &&
			    (cw & 0777) >= 2) {
				copy_dc(bp, name, 7, DC_ALNUM);
				*ui = (bp[17] << 12) | (bp[18] << 6) | bp[19];

				dprint(("id_record: ui 0%o cnt %d\n",
					*ui, cnt));

				/* convert modification date if present */
				if (cnt >= 50 && (cw & 0777) >= 4)
					sprintf(date, "%02d/%02d/%02d.",
						bp[44]+70, bp[45], bp[46]);

				/* extract additional fields if present */
				if (cnt >= 170 && (cw & 0777) >= 16)
					format_catentry(extra, bp+10);

				return RT_PFDUMP;
			}
		}

	}

	/* if 7700 table, extract name and date then skip over it */
	hdr = (bp[0] << 6) | bp[1];
	len = (bp[2] << 6) | bp[3];
	dprint(("id_record: hdr %04o len %d cnt %d\n", hdr, len, cnt));
	if (hdr == 07700 && len*10 + 20 <= cnt) {
		copy_dc(bp+10, name, 7, DC_NOSPC);
		copy_dc(bp+20, date, 10, DC_NONUL);

		/* find comment field */
		if (len >= 14) {
			char *cp, *sp;

			/* old: starts in word 2. */
			/* new: word 2 is time, comment starts in word 7 */
			sp = bp + 30;
			if (is_dc_ts(sp, 057))  /* '.' */
				sp = bp + 80;

			/* skip over date, time, space or zero words */
			for ( ; sp < bp + 110; sp += 10) {

				/* date or time? */
				if (is_dc_ts(sp, 050))  /* '/' */
					continue;
				if (is_dc_ts(sp, 057))  /* '.' */
					continue;

				/* all zero? */
				if (memcmp(sp, "\0\0\0\0\0\0\0\0\0\0", 10) == 0)
					continue;

				/* all spaces? */
				if (memcmp(sp,
				     "\055\055\055\055\055\055\055\055\055\055",
					   10) != 0)
					break;
			}

			/* skip any remaining leading spaces */
			for ( ; sp < bp + 150; sp++)
				if (*sp != 055)
					break;

			copy_dc(sp, extra, MIN(EXTRA_LEN, bp + 150 - sp),
				DC_NONUL);

			/* remove "COPYRIGHT" and trailing spaces */
			cp = strstr(extra, "COPYRIGHT");
			if (!cp)
				cp = extra + strlen(extra);
			for (cp--; cp >= extra && *cp == ' '; cp--)
				;
			*++cp = '\0';
		}

		np += len*10 + 10;
		ncnt -= len*10 + 10;
		hdr = (np[0] << 6) | np[1];
		len = (np[2] << 6) | np[3];
		dprint(("id_record: nxt %04o len %d cnt %d\n", hdr, len, ncnt));
	}

	/* check for PP program */
	if (np[0] && np[1] && np[2] && !np[3] &&   /* name is 3 chars */
	    (np[0] > 26 && np[0] < 37 ||	   /* first char is digit or */
		np[4] || np[5]) &&		   /*   load addr is non-zero */
	    !np[6] && !np[7] &&			   /* middle 12 bits are zero */
	    (np[8] || np[9])) {			   /* length is non-zero */
		copy_dc(np, name, 3, DC_NOSPC);
		return RT_PP;
	}

	switch (hdr) {
	    case 03400:
		return RT_REL;

	    case 05200:
		return RT_PPU;

	    case 05300:
		/* OVL if bit 18 not set */
		if ((np[7] & 040) == 0)
			return RT_OVL;
		/* fall through */
	    case 05100:
		return RT_ABS;

	    case 05400:
		/* ABS if 00,00 overlay */
		if (!np[4] && !np[5])
			return RT_ABS;
		/* fall through */
	    case 05000:
		return RT_OVL;

	    case 06000:
		return RT_CAP;

	    case 06100:
		return RT_PPL;

	    case 07000:
		return RT_OPLD;

	    case 07001:
		return RT_OPL;

	    case 07002:
		return RT_OPLC;

	    case 07600:
		return RT_ULIB;
	}

	/* 7700 table but unrecognized type? */
	if (bp != np)
		return RT_7700;

	copy_dc(bp, name, 7, DC_NOSPC);
	copy_dc(bp, extra, MIN(EXTRA_LEN, cnt), DC_TEXT);
	return RT_TEXT;
}
