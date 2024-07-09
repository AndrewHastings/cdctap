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
 * Output file utility routines.
 */

#include <sys/time.h>
#include <time.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#undef _POSIX_C_SOURCE  /* I didn't set it; who did?? */
#include <fnmatch.h>
#include "cdctap.h"
#include "ifmt.h"
#include "pfdump.h"
#include "outfile.h"

int sout = 0;


/* match pattern as "ui/pat" or "un/pat" or "pat" */
/* returns pat if case-free exact match, name if wildcard match, or NULL */
char *name_match(char *pattern, char *name, int ui)
{
	long want_ui;
	char *pat, *sp, *ep;

	pat = pattern;
	sp = strchr(pattern, '/');

	/* ui or un specified */
	if (sp) {
		pat = sp+1;
		want_ui = strtoul(pattern, &ep, 8);

		/* ui didn't parse, see if valid un */
		if (sp != ep)
			want_ui = un_to_ui(pattern);
		if (ui != want_ui)
			return NULL;
	}

	dprint(("name_match: pat=%s\n", pat));
	if (strcasecmp(pat, name) == 0)
		return pat;
	if (fnmatch(pat, name, FNM_CASEFOLD) == 0)
		return name;
	return NULL;
}


/* parse date into *tm */
/* returns -1 if parse failure */
int parse_date(char *date, struct tm *tm)
{
	int rv;

	rv = sscanf(date, " %d/%d/%d", &tm->tm_year, &tm->tm_mon, &tm->tm_mday);

	/* Ensure invalid date if parse failed */
	if (rv < 3) {
		tm->tm_mday = 0;
		return -1;
	}

	tm->tm_mon--;	/* months are zero-based */
	if (tm->tm_year < 60)
		tm->tm_year += 100;

	dprint(("parse-date: parsed %s\n", date));
	return 0;
}


void set_mtime(char *fname, struct tm *tm)
{
	struct timeval times[2];

	if (!fname[0])
		return;

	tm->tm_isdst = -1;
	times[1].tv_sec = mktime(tm);
	if (times[1].tv_sec == -1) {
		fprintf(stderr, "%s: mtime invalid\n", fname);
		return;
	}

	times[0].tv_sec = time(NULL);
	times[0].tv_usec = times[1].tv_usec = 0;
	if (utimes(fname, times) < 0) {
		fprintf(stderr, "%s: ", fname);
		perror("utimes");
	}
}


/* actual file name returned in fname */
FILE *out_open(char *name, char *sfx, char *fname)
{
	int i;
	FILE *rv;

	if (sout) {
		fname[0] = '\0';
		return stdout;
	}

	sprintf(fname, "%s.%s", name, sfx);
	for (i = 0; i < 100; i++) {
		rv = fopen(fname, "wx");
		if (rv) {
			printf("Extracting to %s\n", fname);
			break;
		}
		if (errno != EEXIST) {
			perror(fname);
			break;
		}
		sprintf(fname, "%s.%d.%s", name, i+1, sfx);
	}

	return rv;
}


void out_close(FILE *of)
{
	if (of == stdout)
		return;

	fclose(of);
}
