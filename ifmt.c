/*
 * Copyright 2024 Andrew B. Hastings. All rights reserved.
 *
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


/* reading if tbuf != NULL, else writing (nbytes, cbufp ignored) */
/* return: -1=EOF, -2=failure, else number of CDC chars unpacked */
int cdc_ctx_init(cdc_ctx_t *cd, TAPE *tap, char *tbuf, int nbytes, char **cbufp)
{
	int nwords, rv;

	memset(cd, 0, sizeof(cdc_ctx_t));
	cd->cd_tap = tap;

	if (tbuf) {
		*cbufp = NULL;
		if (tap_is_write(tap)) {
			fprintf(stderr, "cdc_ctx_init: attempt to read "
					"tape open for writing\n");
			return -2;
		}
		nwords = nbytes*8/60;
		cd->cd_cbuf = malloc(nwords*10 + 9);
		if (!cd->cd_cbuf) {
			fprintf(stderr, "cdc_ctx_init: block size %d "
					"(CDC words %d) too large\n",
				nbytes, nwords);
			return -2;
		}
		rv = unpack6(cd->cd_cbuf, tbuf, nbytes);
		if (rv / 10 != nwords) {
			fprintf(stderr,
				"cdc_ctx_init: unpack6: expected %d, got %d\n",
				nwords * 10, rv);
			free(cd->cd_cbuf);
			cd->cd_cbuf = NULL;
			return -2;
		}
		if (rv == 8 && (cd->cd_cbuf[7] & 017) == 017) {
			free(cd->cd_cbuf);
			cd->cd_cbuf = NULL;
			return -1;
		}
		cd->cd_nchar = cd->cd_nleft = nwords * 10;
		cd->cd_reclen = nwords;
		*cbufp = cd->cd_cbuf;
	} else {
		if (!tap_is_write(tap)) {
			fprintf(stderr, "cdc_ctx_init: attempt to write "
					"tape open for reading\n");
			return -2;
		}
		cd->cd_cbuf = malloc(CDC_CBUFSZ);
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
	char *unused;
	
	if (tap_is_write(cd->cd_tap)) {
		fprintf(stderr, "cdc_skipr: attempt to read "
				"tape open for writing\n");
		return -1;
	}

	while (cd->cd_nchar >= CDC_CBUFSZ) {
		nbytes = tap_readblock(cd->cd_tap, &unused);
		if (nbytes == -2)
			return -1;
		if (nbytes < 0)
			break;

		nwords = nbytes*8/60;
		cd->cd_nchar = nwords * 10;
		cd->cd_reclen += nwords;
	}
	cd->cd_nleft = 0;

	return cd->cd_reclen;
}


/* get next CDC word after advancing nskip words */
/* returns NULL if EOR or error */
char *cdc_skipwords(cdc_ctx_t *cd, int nskip)
{
	ssize_t nbytes;
	int rv, nwords, cskip = nskip * 10;
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

		/* unpack to 6-bit characters */
		nwords = nbytes*8/60;
		cd->cd_nleft = cd->cd_nchar = nwords * 10;
		cd->cd_reclen += nwords;
		rv = unpack6(cd->cd_cbuf, tbuf, nbytes);
		if (rv / 10 != nwords) {
			fprintf(stderr,
				"cdc_skipwords: unpack6: expected %d, got %d\n",
				nwords * 10, rv);
			return NULL;
		}
		dprint(("cdc_skipwords: unpacked %d chars\n", rv));
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
	int nbytes, tmp;

	if (!tap_is_write(cd->cd_tap)) {
		fprintf(stderr, "cdc_flushblock: attempt to write "
				"tape open for reading\n");
		return -1;
	}

	nbytes = pack6(cd->cd_tbuf, cd->cd_cbuf, cd->cd_nchar);
	if (nbytes + 6 > CDC_TBUFSZ) {
		fprintf(stderr, "cdc_flushblock: buffer overflow, nbytes %d\n",
				nbytes);
		exit(3);
	}

	/* append CDC block trailer */
	cp = cd->cd_tbuf + nbytes;
	tmp = (cd->cd_nchar + 8) / 2;
	cp[0] = (tmp >> 4);
	cp[1] = (tmp & 0xf) << 4;
	tmp = cd->cd_blocknum;
	cp[1] |= (tmp >> 20) & 0xf;
	cp[2] = (tmp >> 12) & 0xff;
	cp[3] = (tmp >> 4) & 0xff;
	cp[4] = (tmp & 0xf) << 4;
	cp[5] = eof ? 017 : 0;
	nbytes += 6;
	dprint(("cdc_flushblock: nchar %d bn %d %02x%02x%02x%02x%02x%02x\n",
		cd->cd_nchar, cd->cd_blocknum,
		cp[0], cp[1], cp[2], cp[3], cp[4], cp[5]));

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
