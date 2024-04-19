/*
 * Copyright 2024 Andrew B. Hastings. All rights reserved.
 *
 */

/*
 * ANSI tape label routines.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "ansi.h"


int is_label(char *buf, int nbytes)
{
	return nbytes == 80 &&
	       (strncmp(buf, "VOL", 3) == 0 ||
	        strncmp(buf, "HDR", 3) == 0 ||
	        strncmp(buf, "EOV", 3) == 0 ||
	        strncmp(buf, "EOF", 3) == 0);
}


void print_lfield(char *txt, char *sp, char *ep)
{
	char c, prev = '\0';

	/* trim leading and trailing spaces */
	while (sp <= ep && *sp == ' ')
		sp++;
	while (ep >= sp && *ep == ' ')
		ep--;
	if (sp > ep)
		return;

	printf("%s", txt);
	while (sp <= ep) {
		c = *sp;
		/* compress multiple spaces to one */
		if (prev != ' ' || c != ' ')
			putchar(c >= 32 && c < 127 ? c : '~');
		prev = c;
		sp++;
	}
}


void print_jdate(char *txt, char *sp)
{
	static char days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	int i, yr, jday;

	/* must be all digits, except first may be space */
	for (i = 5; i >= 0; i--)
		if (!isdigit(sp[i]))
			break;
	if (i > 0 || i == 0 && sp[0] != ' ') {
		print_lfield(txt, sp, sp+5);
		return;
	}

	if (sp[0] == ' ')
		yr = 1900;
	else
		yr = 2000 + 100 * (sp[0]-'0');
	yr += 10*(sp[1]-'0') + sp[2]-'0';
	days[1] = 28 + (yr % 4 == 0);

	jday = 100*(sp[3]-'0') + 10*(sp[4]-'0') + sp[5]-'0';
	for (i = 0; i < 12; i++) {
		if (jday - days[i] < 0)
			break;
		jday -= days[i];
	}
	
	/* invalid Julian date? */
	if (i == 12) {
		print_lfield(txt, sp, sp+5);
		return;
	}

	printf("%s%04d/%02d/%02d", txt, yr, i+1, jday);
}


void print_label(char *bp)
{
	print_lfield("", bp, bp+3);

	/* VOL1 */
	if (*bp == 'V') {
		print_lfield(" ", bp+4, bp+9);
		print_lfield(" l", bp+79, bp+79);
		print_lfield(" owner=", bp+37, bp+50);
		print_lfield(" os=", bp+24, bp+36);
		putchar('\n');
		return;
	}

	/* All others */
	print_lfield(" ", bp+4, bp+20);
	print_lfield(" s", bp+31, bp+34);
	print_lfield(" g", bp+35, bp+38);
	print_lfield(" v", bp+39, bp+40);
	print_lfield(" b", bp+54, bp+59);
	print_jdate(" cre=", bp+41);
	print_jdate(" exp=", bp+47);
	print_lfield(" os=", bp+60, bp+72);
	putchar('\n');
}
