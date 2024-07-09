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
#define DC_TEXT   8	/* text lines with CDC line terminators */
void copy_dc(char *src, char *dest, int max, int flags);

int is_dc_ts(char *sp, char sep);
void dump_dword(char *cbuf, int nchar);
void print_data(char *cbuf, int nchar);

#endif /* _DCODE_H */
