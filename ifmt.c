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
 * CDC tape handling routines.
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cdctap.h"
#include "ifmt.h"
#include "simtap.h"

#define CDC_CBUFSZ      (512*10)
#define CDC_TBUFSZ      (CDC_CBUFSZ*6/8+6)

int cdc_flushblock(cdc_ctx_t *cd, int eof);


int unpack6(char *dst, char *src, int nbytes)
{
	int sc, dc;

	sc = dc = 0;
	while (sc+2 < nbytes) {
		dst[dc]   = src[sc]>>2 & 077;
		dst[dc+1] = (src[sc] & 03) << 4 | (src[sc+1] >> 4) & 017;
		dst[dc+2] = (src[sc+1] & 017) << 2 | (src[sc+2] >> 6) & 03;
		dst[dc+3] = src[sc+2] & 077;
		dc += 4;
		sc += 3;
	}
	if (sc < nbytes) {
		dprint(("unpack6: 1\n"));
		dst[dc]   = src[sc]>>2 & 077;
		dc++;
	}
	if (sc < nbytes) {
		dprint(("unpack6: 2\n"));
		dst[dc]   = (src[sc] & 03) << 4 | (src[sc+1] >> 4) & 017;
		dc++;
		sc++;
	}

	return dc;
}


int pack6(char *dst, char *src, int nchar)
{
	int sc, dc;

	dprint(("pack6: nchar %d\n", nchar));
	sc = dc = 0;
	while (sc + 3 < nchar) {
		dst[dc]   = (src[sc] << 2) | (src[sc+1] >> 4) & 03;
		dst[dc+1] = (src[sc+1] & 017) << 4 | (src[sc+2] >> 2) & 017;
		dst[dc+2] = (src[sc+2] & 03) << 6 | src[sc+3];
		dc += 3;
		sc += 4;
	}
	if (sc < nchar) {
		dprint(("pack6: 1\n"));
		dst[dc]   = src[sc] << 2;
		dc++;
		sc++;
	}
	if (sc < nchar) {
		dprint(("pack6: 2\n"));
		dst[dc-1] = dst[dc-1] | (src[sc] >> 4) & 03;
		dst[dc]   = (src[sc] & 017) << 4;
		dc++;
		sc++;
	}
	if (sc < nchar) {
		dprint(("pack6: 3\n"));
		dst[dc-1] = dst[dc-1] | (src[sc] >> 2) & 017;
		dst[dc]   = (src[sc] & 03) << 6;
		dc++;
		sc++;
	}
	return dc;
}


/* returns -2 on failure, else number of CDC chars unpacked */
int unpack_iblock(cdc_ctx_t *cd, char *tbuf, int nbytes)
{
	int rv, nwords, nchar, PPwords;
	char *cp;

	/* unpack entire tape block including trailer */
	nchar = nbytes * 8 / 6;
	cd->cd_cbuf = realloc(cd->cd_cbuf, nchar + 9);
	cd->cd_nchar = 0;
	if (!cd->cd_cbuf) {
		fprintf(stderr, "unpack_iblock: block size %d "
				"(CDC words %d) too large\n",
			nbytes, nchar / 10);
		return -2;
	}
	rv = unpack6(cd->cd_cbuf, tbuf, nbytes);
	if (rv / 10 != nchar / 10) {
		fprintf(stderr,
			"unpack_iblock: unpack6: expected %d, got %d\n",
			nchar, rv);
		free(cd->cd_cbuf);
		cd->cd_cbuf = NULL;
		return -2;
	}

	/*
	 * Determine number of data words.
	 * - no valid trailer: all full words in block
	 * - valid trailer: ignore 48-bit trailer and padding
	 * If present, the trailer immediately follows the last data word and
	 * the entire block is padded to a 24-bit boundary.
	 * (See comment in cdc_flushblock.)
	 */
	nwords = (nbytes - 6) * 8 / 60;
	cp = cd->cd_cbuf + nwords*10;
	PPwords = (nwords*10 + 8) / 2;
	if (cp[0] != (PPwords >> 6) || cp[1] != (PPwords & 077) || cp[6]) {
		dprint(("unpack_iblock: trailer sz 0%o expected 0%o z 0%o\n",
			cp[0] << 6 | cp[1], PPwords, cp[6]));
		nwords = rv / 10;
	}

	cd->cd_nleft = cd->cd_nchar = nwords * 10;
	cd->cd_reclen += nwords;
	return rv;
}


/* reading if tbuf != NULL, else writing (nbytes, cbufp ignored) */
/* return: -1=EOF, -2=failure, else number of CDC chars unpacked */
int cdc_ctx_init(cdc_ctx_t *cd, TAPE *tap, char *tbuf, int nbytes, char **cbufp)
{
	int rv;

	memset(cd, 0, sizeof(cdc_ctx_t));
	cd->cd_tap = tap;

	if (tbuf) {
		*cbufp = NULL;
		if (tap_is_write(tap)) {
			fprintf(stderr, "cdc_ctx_init: attempt to read "
					"tape open for writing\n");
			return -2;
		}

		rv = unpack_iblock(cd, tbuf, nbytes);
		if (rv < 0)
			return rv;

		if (rv == 8 && cd->cd_cbuf[7] == 017) {
			free(cd->cd_cbuf);
			cd->cd_cbuf = NULL;
			return -1;
		}
		*cbufp = cd->cd_cbuf;
	} else {
		if (!tap_is_write(tap)) {
			fprintf(stderr, "cdc_ctx_init: attempt to write "
					"tape open for reading\n");
			return -2;
		}
		cd->cd_cbuf = malloc(CDC_CBUFSZ + 8);
		cd->cd_tbuf = malloc(CDC_TBUFSZ);
		if (!cd->cd_cbuf || !cd->cd_tbuf) {
			free(cd->cd_cbuf);
			fprintf(stderr, "cdc_ctx_init: out of memory "
					"for writing\n");
			return -2;
		}
	}

	return cd->cd_nchar;
}


void cdc_ctx_fini(cdc_ctx_t *cd)
{
	if (tap_is_write(cd->cd_tap) && cd->cd_nchar) {
		dprint(("cdc_ctx_fini: %d char unwritten\n", cd->cd_nchar));
		cdc_flushblock(cd, 0);
	}
	if (cd->cd_tbuf)
		free(cd->cd_tbuf);
	if (cd->cd_cbuf)
		free(cd->cd_cbuf);
}


/* skip over tape blocks until CDC EOR */
/* returns record size in CDC words, negative if error */
int cdc_skipr(cdc_ctx_t *cd)
{
	ssize_t nbytes;
	int nwords;
	char *tbuf;
	
	if (tap_is_write(cd->cd_tap)) {
		fprintf(stderr, "cdc_skipr: attempt to read "
				"tape open for writing\n");
		return -1;
	}

	while (cd->cd_nchar >= CDC_CBUFSZ) {
		nbytes = tap_readblock(cd->cd_tap, &tbuf);
		if (nbytes == -2)
			return -1;
		if (nbytes < 0)
			break;

		if (nbytes * 8 / 6 < CDC_CBUFSZ) {
			/* unpack partial block to get actual data size */
			(void) unpack_iblock(cd, tbuf, nbytes);

		} else {
			/* full block */
			nwords = nbytes * 8 / 60;
			cd->cd_nchar = nwords * 10;
			cd->cd_reclen += nwords;
		}
	}
	cd->cd_nleft = 0;

	return cd->cd_reclen;
}


/* get next CDC word after advancing nskip words */
/* returns NULL if EOR or error */
char *cdc_skipwords(cdc_ctx_t *cd, int nskip)
{
	ssize_t nbytes;
	int rv, cskip = nskip * 10;
	char *tbuf;

	dprint(("cdc_skipwords: skip %d words\n", nskip));
	if (tap_is_write(cd->cd_tap)) {
		fprintf(stderr, "cdc_skipwords: attempt to read "
				"tape open for writing\n");
		return NULL;
	}

	/* read next tape block(s) as needed */
	while (cd->cd_nleft < cskip + 10) {
		dprint(("cdc_skipwords: nleft=%d, refill\n", cd->cd_nleft));

		/* skip remaining chars, ignoring any partial word at end */
		cskip -= cd->cd_nleft / 10 * 10;
		cd->cd_nleft = 0;

		/* if EOR, stop */
		if (cd->cd_nchar < CDC_CBUFSZ) {
			dprint(("cdc_skipwords: EOR\n"));
			cd->cd_nchar = 0;
			return NULL;
		}

		/* read next tape block */
		nbytes = tap_readblock(cd->cd_tap, &tbuf);
		dprint(("cdc_skipwords: readblock returned %ld\n", nbytes));
		if (nbytes < 0) {
			cd->cd_nchar = 0;
			return NULL;
		}

		rv = unpack_iblock(cd, tbuf, nbytes);
		dprint(("cdc_skipwords: unpacked %d chars\n", rv));
		if (rv < 0)
			return NULL;
	}

	dprint(("cdc_skipwords: skipping %d chars\n", cskip));
	cd->cd_nleft -= cskip;
	return cd->cd_cbuf + cd->cd_nchar - cd->cd_nleft;
}


/* get next CDC word */
char *cdc_getword(cdc_ctx_t *cd)
{
	char *rv;

	rv = cdc_skipwords(cd, 0);

	/* consume word if found */
	if (rv) {
		dprint(("cdc_getword: nleft %d\n", cd->cd_nleft));
		cd->cd_nleft -= 10;
	}

	return rv;
}

/* write accumulated CDC chars */
/* returns 0 on success, -1 if error */
int cdc_flushblock(cdc_ctx_t *cd, int eof)
{
	unsigned char *cp;
	int nchar, nbytes;
	int pad, tmp;

	if (!tap_is_write(cd->cd_tap)) {
		fprintf(stderr, "cdc_flushblock: attempt to write "
				"tape open for reading\n");
		return -1;
	}

	/*
	 * Per 60459690D p. J-2, append 48-bit block trailer:
	 *  12 | count of 12-bit PP words including trailer
	 *  24 | # blocks since HDR1
	 *   8 | zero
	 *   4 | 0=EOR, 017=EOF
	 */

	nchar = cd->cd_nchar + 8;
	pad = (4 - (nchar & 3)) & 3; /* to even number of PP words (4 chars) */
	if (nchar + pad > CDC_CBUFSZ + 8) {
		fprintf(stderr, "cdc_flushblock: buffer overflow adding "
				"trailer, %d chars\n", nchar + pad);
		exit(3);
	}

	cp = cd->cd_cbuf + cd->cd_nchar;
	tmp = nchar / 2;
	cp[0] = (tmp >> 6)  & 077;
	cp[1] =  tmp        & 077;
	tmp = cd->cd_blocknum;
	cp[2] = (tmp >> 18) & 077;
	cp[3] = (tmp >> 12) & 077;
	cp[4] = (tmp >> 6)  & 077;
	cp[5] =  tmp        & 077;
	cp[6] = 0;
	cp[7] = eof ? 017 : 0;
	memset(cp+8, 0, pad);

	dprint(("cdc_flushblock: nchar %d pad %d bn %d "
		"%02o%02o%02o%02o%02o%02o%02o%02o\n",
		nchar, pad, cd->cd_blocknum,
		cp[0], cp[1], cp[2], cp[3], cp[4], cp[5], cp[6], cp[7]));

	nbytes = pack6(cd->cd_tbuf, cd->cd_cbuf, nchar+pad);
	if (nbytes > CDC_TBUFSZ) {
		fprintf(stderr, "cdc_flushblock: buffer overflow, nbytes %d\n",
				nbytes);
		exit(3);
	}

	(void) tap_writeblock(cd->cd_tap, cd->cd_tbuf, nbytes);
	cd->cd_nchar = 0;
	cd->cd_blocknum++;
	return 0;
}


int cdc_putword(cdc_ctx_t *cd, char *cp)
{
	int i;

	if (!tap_is_write(cd->cd_tap)) {
		fprintf(stderr, "cdc_putword: attempt to write "
				"tape open for reading\n");
		return -1;
	}

	if (cd->cd_nchar + 10 > CDC_CBUFSZ) {
		fprintf(stderr, "cdc_putword: buffer overflow\n");
		exit(3);
	}
	for (i = 0; i < 10; i++) {
		cd->cd_cbuf[cd->cd_nchar++] = *cp++;
	}

	if (cd->cd_nchar < CDC_CBUFSZ)
		return 0;
	return cdc_flushblock(cd, 0);
}


int cdc_writer(cdc_ctx_t *cd)
{
	return cdc_flushblock(cd, 0);
}


int cdc_writef(cdc_ctx_t *cd)
{
	return cdc_flushblock(cd, 1);
}
