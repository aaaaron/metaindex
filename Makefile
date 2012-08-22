CC=gcc
LEX=flex
CFLAGS=
#CFLAGS=-g -DDEBUG -Wall
HOME=/usr41/staff/aarona
LIBPATH = -L${HOME}/src/c/libs/all 
INCLUDES = -I${HOME}/src/c/libs/all
#----------------------------------------------------------------------

LIBS	= -lm -lxcgi -lsocket -lnsl -lmysqlclient -lfl

PROG	= metaindex

SRCS  = metaindex.c 

OBJS  = metaindex.o 

DEPENDENCY = flex_doc.fc 

#----------------------------------------------------------------------

.c.o: ${OBJS}
	${CC} ${CFLAGS} ${INCLUDES} -c $<

${PROG}: ${OBJS} ${DEPENDENCY}
	${CC} ${CFLAGS} -o ${PROG} ${OBJS} ${LIBPATH} ${LIBS}

all:${PROG}

install:
	strip ${PROG}

clean:
	rm -f ${PROG}
	rm -f *.o
	rm -f core


