CFLAGS=-Wall -Werror -pipe -Dlinux -D_GNU_SOURCE
LDFLAGS=-lpopt
LD=gcc
DEPFLAGS=$(CPPFLAGS) $(CFLAGS) -MM
MAKEDEPEND=$(CC) $(DEPFLAGS) -o $*.d $<

SOURCES = $(wildcard *.c)

spitool: $(SOURCES:.c=.o)
	$(LD) $(LDFLAGS) -o spitool $(SOURCES:.c=.o)

clean:
	rm -f *.o *.d *~ spitool

%.o: %.c
	@$(MAKEDEPEND)
	$(COMPILE.c) -o $@ $<

%.d: %.c
	$(MAKEDEPEND)

-include $(SOURCES:.c=.d)
