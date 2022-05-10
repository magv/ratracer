FIREFLY_CFLAGS?=-Ifirefly/include
FIREFLY_LDFLAGS?=firefly/lib/libfirefly.a

XCFLAGS=${CFLAGS} \
	-Os -g -std=c++14 -pedantic \
	-Wall -Wextra -Wfatal-errors \
	-pipe -fno-omit-frame-pointer \
	${FIREFLY_CFLAGS}

XLDFLAGS=${LDFLAGS} \
	 ${FIREFLY_LDFLAGS} \
	 -lflint -lmpfr -lgmp -lpthread -lz -ldl

XCFLAGS_STATIC=${XCFLAGS} -Os -s -static \
	-fdata-sections -ffunction-sections -Wl,--gc-sections

XLDFLAGS_STATIC=${XLDFLAGS}

all: ratracer README.md

ratracer: ratracer.cpp ratracer.h ratbox.h
	${CXX} ${XCFLAGS} -o $@ ratracer.cpp ${XLDFLAGS}

ratracer.static: ratracer.cpp ratracer.h ratbox.h
	${CXX} ${XCFLAGS_STATIC} -o $@ ratracer.cpp ${XLDFLAGS_STATIC}

README.md: ratracer.cpp mkmanual.sh
	sed '/MANUAL/{n;q}' $@ >$@.tmp
	./mkmanual.sh >>$@.tmp <$<
	mv $@.tmp $@

clean:
	rm -f ratracer
