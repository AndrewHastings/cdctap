/*
 * Copyright 2024 Andrew B. Hastings. All rights reserved.
 *
 */

/*
 * CDC display code routines.
 */

#ifndef _DCODE_H
#define _DCODE_H 1

extern char dcmap[64];
extern char *c74map[64];
extern char *c76map[64];

#define DC_ALL    0	/* entire buffer */
#define DC_ALNUM  7	/* alphanumeric, null-terminated */
#define DC_NOSPC  6	/* any character, terminate on space or null */
#define DC_NONUL  4	/* any character, terminate on null */
void copy_dc(char *src, char *dest, int max, int flags);

int is_dc_ts(char *sp, char sep);
void dump_dword(char *cbuf, int nchar);
void print_data(char *cbuf, int nchar);

#endif /* _DCODE_H */
