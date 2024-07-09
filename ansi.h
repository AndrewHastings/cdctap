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
 * ANSI tape label routines.
 */

#ifndef _ANSI_H
#define _ANSI_H 1

extern int is_label(char *buf, int nbytes, char *lbuf);
extern void print_label(char *bp);
extern void print_lfield(char *txt, char *sp, char *ep);
extern void print_jdate(char *txt, char *sp);

#endif /* _ANSI_H */
