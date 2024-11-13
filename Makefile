#
# Copyright 2024 Andrew B. Hastings. All rights reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# version 2, as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#

CFLAGS=-g -fsanitize=address -Werror -Wunused-variable

HDRS = ansi.h cdctap.h dcode.h ifmt.h opl.h outfile.h pfdump.h \
       rectype.h simtap.h
OBJS = ansi.o cdctap.o dcode.o ifmt.o opl.o outfile.o pfdump.o \
       rectype.o simtap.o

cdctap: $(OBJS)
	$(CC) $(CFLAGS) -o cdctap $^

clean:
	$(RM) $(OBJS)

clobber:
	$(RM) cdctap $(OBJS)

%.o : %.c $(HDRS)
	$(CC) $(CFLAGS) -c $<
