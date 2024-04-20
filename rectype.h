/*
 * Copyright 2024 Andrew B. Hastings. All rights reserved.
 *
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
    RT_7700,	/* 7700 table, unknown file type */
    RT_OPL,	/* Modify OPL deck */
    RT_OPLC,	/* Modify OPL common deck */
    RT_OPLD,	/* Modify OPL directory */
    RT_PP,	/* PP program */
    RT_PPU,	/* PPU program */
    RT_PPL,	/* 16-bit PP program */
    RT_ULIB,	/* User library */
    RT_REL,	/* Relocatable subprogram */
    RT_ABS,	/* Absolute program */
    RT_OVL,	/* Overlay */
    RT_CAP,	/* Fast dynamic load capsule */
    RT_PFLBL,	/* PFDUMP label */
    RT_PFDUMP,	/* PFDUMP file */
} rectype_t;

extern char *rectype[];
extern rectype_t id_record(char *bp, int cnt, char *name,
			   char *date, char *extra, int *ui);

#define EXTRA_LEN 120

#endif /* _RECTYPE_H */
