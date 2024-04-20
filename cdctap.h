/*
 * Copyright 2024 Andrew B. Hastings. All rights reserved.
 *
 */

/*
 * Read CDC I-format tapes in SIMH tape image format.
 */

#define dprint(x)	if (debug) printf x

#define MIN(a, b)	((a) < (b) ? (a) : (b))

extern int debug;
extern int verbose;