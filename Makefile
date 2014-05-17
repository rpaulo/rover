PROG=rover
MAN=

LDADD+=-lpthread -lgpio -lcurses -liic

.include <bsd.prog.mk>
