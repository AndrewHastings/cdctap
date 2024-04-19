/*
 * Copyright 2024 Andrew B. Hastings. All rights reserved.
 *
 */

/*
 * Routines for reading/writing SIMH-format tape images.
 */

#ifndef _SIMTAP_H
#define _SIMTAP_H 1

typedef struct {
	FILE		*tp_fp;
	char		*tp_path;
	char		*tp_buf;	/* only for read mode */
	uint32_t	tp_nbytes;	/* only for read mode */
	uint8_t		tp_status;
} TAPE;

extern TAPE *tap_open(char *path, char *fname);
extern void tap_close(TAPE *tap);
extern int tap_is_write(TAPE *tap);
extern ssize_t tap_readblock(TAPE *tap, char **bufp);
extern ssize_t tap_writeblock(TAPE *tap, char *buf, ssize_t nbytes);

#endif /* _SIMTAP_H */
