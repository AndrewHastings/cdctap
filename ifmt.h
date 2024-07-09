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

#ifndef _IFMT_H
#define _IFMT_H 1

#include "simtap.h"

typedef struct {
	TAPE	*cd_tap;
	char	*cd_cbuf;	/* unpacked tape block */
	int	cd_nchar;	/* # CDC chars in unpacked tape block */
	/* fields only for writing: */
	int	cd_blocknum;
	char	*cd_tbuf;	/* packed tape block */
	/* fields only for reading: */
	int	cd_reclen;	/* accumulated CDC record size in words */
	int	cd_nleft;	/* # CDC chars left to consume from cbuf */
} cdc_ctx_t;

extern int unpack6(char *dst, char *src, int nbytes);
extern int cdc_ctx_init(cdc_ctx_t *cd, TAPE *tap, char *tbuf, int nbytes, char **cbufp);
extern void cdc_ctx_fini(cdc_ctx_t *cd);
extern int cdc_skipr(cdc_ctx_t *cd);
extern char *cdc_skipwords(cdc_ctx_t *cd, int nskip);
extern char *cdc_getword(cdc_ctx_t *cd);
extern int cdc_putword(cdc_ctx_t *cd, char *cp);
extern int cdc_writer(cdc_ctx_t *cd);
extern int cdc_writef(cdc_ctx_t *cd);

#endif /* _IFMT_H */
