/*
 * Copyright 2024 Andrew B. Hastings. All rights reserved.
 *
 */

/*
 * PDFUMP-related routines.
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include "cdctap.h"
#include "dcode.h"
#include "ifmt.h"
#include "outfile.h"
#include "pfdump.h"
#include "simtap.h"


/* VALIDUZ mappings from MECC */
static struct validuz {
	char	*val_un;
	int	val_ui;
} vtab[] = {
    { "UTILITY",    0524 },
    { "SYSLIB",  0377701 },
    { "SYSPROC", 0377702 },
    { "MULTI",   0377703 },
    { "CALLPRG", 0377704 },
    { "WRITEUP", 0377705 },
    { "CHARGE",  0377706 },
    { "LIBRARY", 0377776 },
    { "SYSTEMX", 0377777 },
    { NULL,           -1 }
};


char *ui_to_un(int ui)
{
	struct validuz *vp;

	for (vp = vtab; vp->val_un; vp++) {
		if (ui == vp->val_ui)
			break;
	}
	return vp->val_un;
}


int un_to_ui(char *un)
{
	int i, match;
	struct validuz *vp;

	for (vp = vtab; vp->val_un; vp++) {
		match = 1;
		for (i = 0; un[i] && un[i] != '/'; i++) {
			if (toupper(un[i]) != vp->val_un[i]) {
				match = 0;
				break;
			}
		}
		if (match)
			break;
	}

	dprint(("un_to_ui: un %s ui 0%o\n", un, vp->val_ui));
	return vp->val_ui;
}


void format_pflabel(char *dp, char *sp)
{
	int reel;
	char fam[15];
	char pn[11];

	fam[0] = pn[0] = 0;

	copy_dc(sp+50, fam+8, 7, DC_ALNUM);
	if (fam[8])
		memcpy(fam, " family=", 8);

	copy_dc(sp+60, pn+4, 7, DC_ALNUM);
	if (pn[4])
		memcpy(pn, " PN=", 4);

	reel = (sp[17] << 12) | (sp[18] << 6) | sp[19];
	sprintf(dp, "reel %d mask %03o%s%s",
		     reel, ((sp[28] & 3) << 6) | sp[29], fam, pn);
}



void format_catentry(char *dp, char *sp)
{
	int ui, len;
	char ctbuf[3], *ct = ctbuf;
	char modebuf[7], *mode = modebuf;
	char ssbuf[8], *ss = ssbuf;
	char pw[12];
	char ucw[16];
	char unbuf[11], *un;

	pw[0] = ucw[0] = unbuf[0] = 0;

	ui = (sp[7] << 12) | (sp[8] << 6) | sp[9];
	len = (sp[10] << 18) | (sp[11] << 12) | (sp[12] << 6) | sp[13];

	switch (sp[40]) {
	    case 0:   ct = "P"; break;
	    case 1:   ct = "S"; break;
	    case 2:   ct = "L"; break;
	    default:  sprintf(ct, "%d", sp[40]); break;
	}

	switch (sp[41]) {
	    case 0:   mode = "W"; break;
	    case 1:   mode = "R"; break;
	    case 2:   mode = "A"; break;
	    case 3:   mode = "X"; break;
	    case 4:   mode = "N"; break;
	    case 5:   mode = "M"; break;
	    case 6:   mode = "RM"; break;
	    case 7:   mode = "RA"; break;
	    case 8:   mode = "U"; break;
	    case 9:   mode = "RU"; break;
	    default:  sprintf(mode, "%d", sp[41]); break;
	}

	switch (sp[61]) {
	    case 0:   ss = "NUL"; break;
	    case 1:   ss = "BAS"; break;
	    case 2:   ss = "FOR"; break;
	    case 3:   ss = "FTN"; break;
	    case 4:   ss = "EXE"; break;
	    case 5:   ss = "BAT"; break;
	    case 6:   ss = "MNF"; break;
	    case 7:   ss = "SNO"; break;
	    case 8:   ss = "COB"; break;
	    case 9:   ss = "PAS"; break;
	    case 10:  ss = "ACC"; break;
	    case 11:  ss = "TRN"; break;
	    default:  sprintf(ss, "%d", sp[61]); break;
	}

	if (verbose > 1) {
		if ((un = ui_to_un(ui)) != NULL)
			sprintf(unbuf, " (%s)", un);

		copy_dc(sp+70, pw+4, 7, DC_NONUL);
		if (pw[4])
			memcpy(pw, " pw=", 4);

		if (memcmp(sp+140, "\000\000\000\000\000\000\000\000\000\000",
			   10) != 0) {
			memcpy(ucw, " ucw=", 5);
			copy_dc(sp+140, ucw+5, 10, DC_ALL);
		}
	}

	sprintf(dp, "%6d %-1s %-2s %-3s %6o%s%s%s", len, ct, mode, ss, ui,
						    unbuf, pw, ucw);
}


void analyze_pfdump(cdc_ctx_t *cd)
{
	char *cp;
	char cname[8], dword[20];
	int i, len, lim, max, nread;
	char *btype, *flag;
	static char *types[8] = {
		"label",
		"catalog",
		"permits",
		"data",
		"reelend",
		"catimage",
		"type 6",
		"end",
	};
	static char *flags[8] = {
		"",
		" EOR",
		" EOF",
		" EOI",
		" syssect",
		" flag 5",
		" flag 6",
		" dump",
	};

	switch (verbose) {
	    case 0:   lim = 0; break;
	    case 1:   lim = 8; break;
	    default:  lim = 512; break;
	}

	while (cp = cdc_getword(cd)) {
		copy_dc(cp, cname, 7, DC_ALNUM);
		btype = types[cp[7] & 07];
		flag = flags[(cp[8] >> 3) & 07];
		len = ((cp[8] & 07) << 6) | cp[9];

		printf("%-7s %3d ", cname, len);
		for (i = 0; i < 10; i++)
			printf("%02o", cp[i]);
		printf(" %s%s\n", btype, flag);

		max = MIN(len, lim);
		for (i = 0; i < max; i += nread) {

			/* read even word */
			if (!(cp = cdc_getword(cd)))
				break;
			nread = 1;
			memcpy(dword, cp, 10);

			/* read odd word */
			if (i+1 < max) {
				if (cp = cdc_getword(cd)) {
					nread = 2;
					memcpy(dword+10, cp, 10);
				}
			}

			printf("            ");
			dump_dword(dword, nread*10);
			if (i % 8 == 0)
				printf(" 0%o", i);
			putchar('\n');

			if (!cp)
				break;
		}

		if (!cp) {
			dprint(("analyze_pfdump: premature CDC EOR at "
				"0x%lx\n", ftell(cd->cd_tap->tp_fp)));
			break;
		}

		len -= i;
		dprint(("analyze_pfdump: skip %d\n", len));
		if (!cdc_skipwords(cd, len))
			break;
	}
}


char *extract_pfdump(cdc_ctx_t *cd, char *name)
{
	TAPE *ot = NULL;
	char nbuf[16], fname[24], cname[8];
	char *np = name;
	char *cp, *dp;
	int ui, btype, flag;
	int i, len;
	struct tm tm;
	cdc_ctx_t ocd;

	dprint(("extract_pfdump: %s\n", name));
	memset(&tm, 0, sizeof tm);

	while (cp = cdc_getword(cd)) {

		/* parse PFDUMP control word */
		btype = cp[7] & 07;
		flag = (cp[8] >> 3) & 07;
		len = ((cp[8] & 07) << 6) | cp[9];

		switch (btype) {
		    case 1:		/* catalog entry */
		        /* word 1: name & ui */
			cp = cdc_getword(cd);
			if (!cp)
				goto err;
			if (ot) {
				cdc_ctx_fini(&ocd);
				tap_close(ot);
				copy_dc(cp, cname, 7, DC_ALNUM);
				fprintf(stderr,
					"%s: multiple PFDUMP catalog entries, "
					"found entry for %s\n", name, cname);
				np = cname;
			}
			ui = (cp[7] << 12) | (cp[8] << 6) | cp[9];

			/* skip words 2-3 */
			if (!cdc_skipwords(cd, 2))
				goto err;

			/* word 4: modification date/time */
			cp = cdc_getword(cd);
			if (!cp)
				goto err;
			tm.tm_year  = cp[4] + 70;
			tm.tm_mon   = cp[5] - 1;
			tm.tm_mday  = cp[6];
			tm.tm_hour  = cp[7];
			tm.tm_min   = cp[8];
			tm.tm_sec   = cp[9];
			tm.tm_isdst = -1;
	
			/* use subdir for UN (if known) or ui */
			if ((dp = ui_to_un(ui)) != NULL)
				sprintf(nbuf, "%s", dp);
			else
				sprintf(nbuf, "%o", ui);
			if (mkdir(nbuf, 0777) < 0 && errno != EEXIST) {
				fprintf(stderr, "%s: mkdir: ", np);
				perror(nbuf);
				(void) cdc_skipr(cd);
				return "";
			}
			strcat(nbuf, "/");
			strcat(nbuf, np);

			ot = tap_open(nbuf, fname);
			if (!ot) {
				(void) cdc_skipr(cd);
				return "";
			}
			if (cdc_ctx_init(&ocd, ot, NULL, 0, NULL) < 0) {
				tap_close(ot);
				(void) cdc_skipr(cd);
				return "";
			}

			/* skip remainder of catalog entry */
			len -= 4;
			break;

		    case 3:		/* data */
			/* ignore system sector and other data subtypes */
			if (flag > 3)
				break;

			for (i = 0; i < len; i++) {
				cp = cdc_getword(cd);
				if (!cp)
					goto err;
				if (cdc_putword(&ocd, cp) < 0)
					goto err;
			}

			if (flag == 1)
				cdc_writer(&ocd);
			if (flag == 2)
				cdc_writef(&ocd);

			continue;

		    default:
			/* skip over other types */
			break;
		}

		if (!cdc_skipwords(cd, len))
			break;
	}

	if (!ot)
		return "no catalog entry in PFDUMP record";

	cdc_ctx_fini(&ocd);
	tap_close(ot);
	if (tm.tm_mday)
		set_mtime(fname, &tm);
	return NULL;

    err:
	if (ot) {
		cdc_ctx_fini(&ocd);
		tap_close(ot);
	}
	(void) cdc_skipr(cd);
	return "EOR while extracting PFDUMP";
}
