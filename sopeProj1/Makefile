CFILES := main.c
PROG := sopeProj1
CFLAGS := -Wall -Wextra -g
LDFLAGS :=

CFLAGS += -MMD
CC := gcc

OBJFILES := $(CFILES:.c=.o)
DEPFILES := $(CFILES:.c=.d)

BINDIR := bin

$(BINDIR)/$(PROG) : $(OBJFILES)
		mkdir -p $(BINDIR)
		$(LINK.o) $(LDFLAGS) -o $@ $^
		rm *.d
		rm *.o
clean :
		rm -rf $(BINDIR) $(OBJFILES) $(DEPFILES)

-include $(DEPFILES)
