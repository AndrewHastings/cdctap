/*
 * Copyright 2024 Andrew B. Hastings. All rights reserved.
 *
 */

/*
 * PDFUMP-related routines.
 */

#ifndef _PFDUMP_H
#define _PFDUMP_H 1

extern char *ui_to_un(int ui);
extern int un_to_ui(char *un);
extern void format_pflabel(char *dp, char *sp);
extern void format_catentry(char *dp, char *sp);
extern void analyze_pfdump(cdc_ctx_t *cd);
extern char *extract_pfdump(cdc_ctx_t *cd, char *name);

#endif /* _PFDUMP_H */
