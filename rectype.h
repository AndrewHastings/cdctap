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
 * Identify CDC record type (adapted from COMCSRT).
 */

#ifndef _RECTYPE_H
#define _RECTYPE_H 1

typedef enum {
    RT_EMPTY,	/* Zero-length record */
    RT_EOF,	/* EOF */
    RT_TEXT,	/* Unrecognized */
    RT_PROC,	/* CCL procedure */
    RT_DATA,	/* Arbitrary data */
    RT_7700,	/* 7700 table, unknown file type */
    RT_ACF,	/* MODIFY compressed compile file */
    RT_OPL,	/* MODIFY OPL deck */
    RT_OPLC,	/* MODIFY OPL common deck */
    RT_OPLD,	/* MODIFY OPL directory */
    RT_UCF,	/* UPDATE compressed compile file */
    RT_UPL,	/* UPDATE PL */
    RT_UPLR,	/* UPDATE random PL */
    RT_UPLD,	/* UPDATE random PL directory */
    RT_PP,	/* PP program */
    RT_PPU,	/* PPU program */
    RT_PPL,	/* 16-bit PP program */
    RT_ULIB,	/* User library */
    RT_REL,	/* Relocatable subprogram */
    RT_ABS,	/* Absolute program */
    RT_OVL,	/* Overlay */
    RT_COS,	/* Chippewa OS CP program */
    RT_SDR,	/* Special deadstart record */
    RT_CAP,	/* Fast dynamic load capsule */
    RT_USER,	/* User-defined record (7500 table) */
    RT_DUMPPF,	/* UMinn DUMPPF */
    RT_PFLBL,	/* PFDUMP label */
    RT_PFDUMP,	/* PFDUMP file */
} rectype_t;

extern char *rectype[];
extern rectype_t id_record(char *bp, int cnt, char *name,
			   char *date, char *extra, int *ui);

#define EXTRA_LEN 120

#endif /* _RECTYPE_H */
