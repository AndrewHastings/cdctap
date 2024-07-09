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
