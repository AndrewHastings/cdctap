#
# Copyright 2024 Andrew B. Hastings. All rights reserved.
#

OBJS = ansi.o cdctap.o dcode.o ifmt.o outfile.o pfdump.o rectype.o simtap.o

cdctap: $(OBJS)
	$(CC) $(CFLAGS) -o cdctap $^

clean:
	$(RM) $(OBJS)

clobber:
	$(RM) cdctap $(OBJS)
