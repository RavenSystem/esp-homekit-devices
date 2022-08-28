PROJECT = heatshrink
OPTIMIZE = -O3
WARN = -Wall -Wextra -pedantic #-Werror
WARN += -Wmissing-prototypes
WARN += -Wstrict-prototypes
WARN += -Wmissing-declarations

# If libtheft is available, build additional property-based tests.
# Uncomment these to use it in test_heatshrink_dynamic.
#CFLAGS += -DHEATSHRINK_HAS_THEFT
#THEFT_PATH=	/usr/local/
#THEFT_INC=	-I${THEFT_PATH}/include/
#LDFLAGS += -L${THEFT_PATH}/lib -ltheft

CFLAGS += -std=c99 -g ${WARN} ${THEFT_INC} ${OPTIMIZE}

all: heatshrink test_runners libraries

libraries: libheatshrink_static.a libheatshrink_dynamic.a

test_runners: test_heatshrink_static test_heatshrink_dynamic
test: test_runners
	./test_heatshrink_static
	./test_heatshrink_dynamic
ci: test

clean:
	rm -f heatshrink test_heatshrink_{dynamic,static} \
		*.o *.os *.od *.core *.a {dec,enc}_sm.png TAGS
	rm -rf ${BENCHMARK_OUT}

TAGS:
	etags *.[ch]

diagrams: dec_sm.png enc_sm.png

dec_sm.png: dec_sm.dot
	dot -o $@ -Tpng $<

enc_sm.png: enc_sm.dot
	dot -o $@ -Tpng $<

# Benchmarking
CORPUS_ARCHIVE=	cantrbry.tar.gz
CORPUS_URL=	http://corpus.canterbury.ac.nz/resources/${CORPUS_ARCHIVE}
BENCHMARK_OUT=	benchmark_out

## Uncomment one of these.
DL=	curl -o ${CORPUS_ARCHIVE}
#DL=	wget -O ${CORPUS_ARCHIVE}

bench: heatshrink corpus
	mkdir -p ${BENCHMARK_OUT}
	cd ${BENCHMARK_OUT} && tar vzxf ../${CORPUS_ARCHIVE}
	time ./benchmark

corpus: ${CORPUS_ARCHIVE}

${CORPUS_ARCHIVE}:
	${DL} ${CORPUS_URL}

# Installation
PREFIX ?=	/usr/local
INSTALL ?=	install
RM ?=		rm

install: libraries heatshrink
	${INSTALL} -c heatshrink ${PREFIX}/bin/
	${INSTALL} -c libheatshrink_static.a ${PREFIX}/lib/
	${INSTALL} -c libheatshrink_dynamic.a ${PREFIX}/lib/
	${INSTALL} -c heatshrink_common.h ${PREFIX}/include/
	${INSTALL} -c heatshrink_config.h ${PREFIX}/include/
	${INSTALL} -c heatshrink_encoder.h ${PREFIX}/include/
	${INSTALL} -c heatshrink_decoder.h ${PREFIX}/include/

uninstall:
	${RM} -f ${PREFIX}/lib/libheatshrink_static.a
	${RM} -f ${PREFIX}/lib/libheatshrink_dynamic.a
	${RM} -f ${PREFIX}/include/heatshrink_common.h
	${RM} -f ${PREFIX}/include/heatshrink_config.h
	${RM} -f ${PREFIX}/include/heatshrink_encoder.h
	${RM} -f ${PREFIX}/include/heatshrink_decoder.h

# Internal targets and rules

OBJS = heatshrink_encoder.o heatshrink_decoder.o

DYNAMIC_OBJS= $(OBJS:.o=.od)
STATIC_OBJS=  $(OBJS:.o=.os)

DYNAMIC_LDFLAGS= ${LDFLAGS} -L. -lheatshrink_dynamic
STATIC_LDFLAGS= ${LDFLAGS} -L. -lheatshrink_static

# Libraries should be built separately for versions
# with and without dynamic allocation.
CFLAGS_STATIC = ${CFLAGS} -DHEATSHRINK_DYNAMIC_ALLOC=0
CFLAGS_DYNAMIC = ${CFLAGS} -DHEATSHRINK_DYNAMIC_ALLOC=1

heatshrink: heatshrink.od libheatshrink_dynamic.a
	${CC} -o $@ $^ ${CFLAGS_DYNAMIC} -L. -lheatshrink_dynamic

test_heatshrink_dynamic: test_heatshrink_dynamic.od test_heatshrink_dynamic_theft.od libheatshrink_dynamic.a
	${CC} -o $@ $< ${CFLAGS_DYNAMIC} test_heatshrink_dynamic_theft.od ${DYNAMIC_LDFLAGS}

test_heatshrink_static: test_heatshrink_static.os libheatshrink_static.a
	${CC} -o $@ $< ${CFLAGS_STATIC} ${STATIC_LDFLAGS}

libheatshrink_static.a: ${STATIC_OBJS}
	ar -rcs $@ $^

libheatshrink_dynamic.a: ${DYNAMIC_OBJS}
	ar -rcs $@ $^

%.od: %.c
	${CC} -c -o $@ $< ${CFLAGS_DYNAMIC}

%.os: %.c
	${CC} -c -o $@ $< ${CFLAGS_STATIC}

*.os: Makefile *.h
*.od: Makefile *.h

