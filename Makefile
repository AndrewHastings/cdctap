#
# Copyright 2024 Andrew B. Hastings. All rights reserved.
#

CFLAGS=-g -fsanitize=address

HDRS = ansi.h cdctap.h dcode.h ifmt.h outfile.h pfdump.h rectype.h simtap.h
OBJS = ansi.o cdctap.o dcode.o ifmt.o outfile.o pfdump.o rectype.o simtap.o

cdctap: $(OBJS)
	$(CC) $(CFLAGS) -o cdctap $^

clean:
	$(RM) $(OBJS)

clobber:
	$(RM) cdctap $(OBJS)

%.o : %.c $(HDRS)
	$(CC) $(CFLAGS) -c $<
