/*
 * Copyright 2024 Andrew B. Hastings. All rights reserved.
 *
 */

/*
 * CDC display code routines.
 */

#include <stdio.h>
#include "cdctap.h"
#include "dcode.h"

char dcmap[64] = {
	':', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', '0', '1', '2', '3', '4',
	'5', '6', '7', '8', '9', '+', '-', '*',
	'/', '(', ')', '$', '=', ' ', ',', '.',
	'#', '[', ']', '%', '"', '_', '!', '&',
	'\'', '?', '<', '>', '@', '\\', '^', ';'
};
char *c74map[64] = {
	"@:", "@", "^", "@C", ":", "@E", "@F", "`",
	"@H", "@I", "@J", "@K", "@L", "@M", "@N", "@O",
	"@P", "@Q", "@R", "@S", "@T", "@U", "@V", "@W",
	"@X", "@Y", "@Z", "@0", "@1", "@2", "@3", "@4",
	"@5", "@6", "@7", "@8", "@9", "@+", "@-", "@*",
	"@/", "@(", "@)", "@$", "@=", "@ ", "@,", "@.",
	"@#", "@[", "@]", "@%", "@\"", "@_", "@!", "@&",
	"@'", "@?", "@<", "@>", "@@", "@\\", "@^", "@;"
};
char *c76map[64] = {
	"^:", "a", "b", "c", "d", "e", "f", "g",
	"h", "i", "j", "k", "l", "m", "n", "o",
	"p", "q", "r", "s", "t", "u", "v", "w",
	"x", "y", "z", "{", "|", "}", "~", "\177",
	"\0", "\001", "\002", "\003", "\004", "\005", "\006", "\007",
	"\010", "\011", "\012", "\013", "\014", "\015", "\016", "\017",
	"\020", "\021", "\022", "\023", "\024", "\025", "\026", "\027",
	"\030", "\031", "\032", "\033", "\034", "\035", "\036", "\037"
};


/* print two CDC words in octal and display code */
void dump_dword(char *cbuf, int nchar)
{
	int i;

	/* print 60-bit words as octal */
	for (i = 0; i < 20; i++) {
		if (i < nchar)
			printf("%02o", cbuf[i]);
		else
			printf("  ");
		if (i % 10 == 9)
			putchar(' ');
	}

	/* print 60-bit words as display code */
	for (i = 0; i < 20; i++) {
		if (i < nchar)
			putchar(dcmap[cbuf[i]]);
		else
			putchar(' ');
		if (i % 20 == 9)
			putchar(' ');
	}
}


void print_data(char *cbuf, int nchar)
{
	int i, lim;

	switch (verbose) {
	    case 0:   lim = 20; break;
	    case 1:   lim = 160; break;
	    default:  lim = nchar; break;
	}
	lim = MIN(nchar, lim);

	dprint(("print_data: nchar %d lim %d\n", nchar, lim));
	for (i = 0; i < lim; i += 20){
		if (i)
			printf("      ");

		dump_dword(cbuf+i, MIN(20, lim-i));

		if (i == 0)
			printf(" [%d]", nchar);
		else if (i % 80 == 0)
			printf(" 0%o", i / 10);
		putchar('\n');
	}
}


void copy_dc(char *sp, char *dp, int max, int flags)
{
	int i, j, k;
	char c;

	dprint(("copy_dc: max %d flags %d dp %p\n", max, flags, dp));
	for (i = 0; i < max; i++) {
		c = sp[i];
		/* skip EOL if requested */
		if ((flags & 8) && !c) {
			j = i / 10 * 10;    /* start of word */
			if (i - j == 9)
				j += 10;    /* also scan next word */
			j = MIN(j+10, max); /* end of word */
			for (k = i+1; k < j; k++)
				if (sp[k])
					break;
			dprint(("copy_dc: i=%d j=%d k=%d dp %p\n", i, j, k, dp));
			if (k == j) {	   /* all null, EOL found */
				dprint(("copy_dc: EOL\n"));
				if (j + 2 < max) {
					*dp++ = ' ';
					*dp++ = ' ';
				}
				i = j - 1; /* continue on next line */
				continue;
			}

		}
		/* stop on non-alphanumeric if requested */
		if ((flags & 1) && c > 36)
			break;
		/* stop on space if requested */
		if ((flags & 2) && c == 055)
			break;
		/* stop on null if requested */
		if ((flags & 4) && !c)
			break;
		*dp++ = dcmap[c];
	}
	*dp++ = '\0';
}


/* Check for "yy/mm/dd." or "hh.mm.ss." in display code */
int is_dc_ts(char *sp, char sep)
{
	/* skip initial space, if any */
	if (sp[0] == 055)
		sp++;

	/* separators in proper positions? */
	if (sp[2] != sep || sp[5] != sep ||
	    sp[8] != 057)	/* '.' */
		return 0;

	/* digits in proper positions? */
	if (sp[0] <= 26 || sp[0] > 36 ||
	    sp[1] <= 26 || sp[1] > 36 ||
	    sp[3] <= 26 || sp[3] > 36 ||
	    sp[4] <= 26 || sp[4] > 36 ||
	    sp[6] <= 26 || sp[6] > 36 ||
	    sp[7] <= 26 || sp[7] > 36)
		return 0;

	return 1;
}
