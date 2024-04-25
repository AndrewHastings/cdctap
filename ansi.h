/*
 * Copyright 2024 Andrew B. Hastings. All rights reserved.
 *
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
