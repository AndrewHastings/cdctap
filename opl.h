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
 * MODIFY and UPDATE PL routines.
 */

#ifndef _OPL_H
#define _OPL_H 1

extern char *extract_opl(cdc_ctx_t *cd, char *name);
extern char *extract_upl(cdc_ctx_t *cd, char *name, struct tm *tm);
extern char *extract_uplr(cdc_ctx_t *cd, char *name, struct tm *tm);

#endif /* _OPL_H */
