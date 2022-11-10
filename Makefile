XCFLAGS=${CFLAGS} \
	-Ibuild/include \
	-O3 -g -std=c++14 -fopenmp \
	-Wall -Wextra -Wfatal-errors \
	-pipe -fno-omit-frame-pointer -fdata-sections -ffunction-sections

XLDFLAGS=${LDFLAGS} \
	-Lbuild/lib -Wl,--gc-sections \
	-lfirefly -lflint -lmpfr -lgmp -lpthread -lz -ldl -ljemalloc

all: ratracer README.md doc/commands.tex

download: build/jemalloc.tar.bz2 build/gmp.tar.xz build/mpfr.tar.xz build/flint.tar.gz build/zlib.tar.xz build/firefly.tar.bz2 phony

deps: build/jemalloc.done build/gmp.done build/mpfr.done build/flint.done build/zlib.done build/firefly.done phony

docs: doc/ratracer.pdf phony

clean: phony
	rm -rf build/ ratracer ratracer.static doc/ratracer.pdf

check: ratracer phony
	./check

bench: ratracer phony
	@./bench

phony:;

README.md: ratracer.cpp mkmanual.sh
	sed '/MANUAL/{n;q}' $@ >$@.tmp
	./mkmanual.sh >>$@.tmp <$<
	mv $@.tmp $@

doc/commands.tex: ratracer.cpp doc/mklatex.sh
	./doc/mklatex.sh >$@ <$<

doc/ratracer.pdf: doc/ratracer.lyx doc/commands.tex doc/preamble.tex doc/ratracer.bib
	lyx --export-to pdf2 $@ $<

doc/ratracer.tex: doc/ratracer.lyx doc/commands.tex doc/preamble.tex doc/ratracer.bib
	lyx --export-to pdflatex $@.tmp $<
	sed -e '/documentclass/a\\\\pdfoutput=1' -e '/^%/d' $@.tmp >$@
	rm -f $@.tmp

build/.dir:
	mkdir -p build
	date >$@

build/jemalloc.tar.bz2: build/.dir
	wget --no-use-server-timestamps -qO $@ \
		"https://github.com/jemalloc/jemalloc/releases/download/5.3.0/jemalloc-5.3.0.tar.bz2"

build/gmp.tar.xz: build/.dir
	wget --no-use-server-timestamps -qO $@ \
		"https://gmplib.org/download/gmp/gmp-6.2.1.tar.xz"

build/mpfr.tar.xz: build/.dir
	wget --no-use-server-timestamps -qO $@ \
		"https://www.mpfr.org/mpfr-current/mpfr-4.1.0.tar.xz"

build/flint.tar.gz: build/.dir
	wget --no-use-server-timestamps -qO $@ \
		"http://flintlib.org/flint-2.9.0.tar.gz"

build/zlib.tar.xz: build/.dir
	wget --no-use-server-timestamps -qO $@ \
		"http://zlib.net/zlib-1.2.13.tar.xz"

build/firefly.tar.bz2: build/.dir
	wget --no-use-server-timestamps -qO $@ \
		"https://gitlab.com/firefly-library/firefly/-/archive/bb-per-thread/firefly-bb-per-thread.tar.bz2"

BUILD=${CURDIR}/build

build/jemalloc.done: build/jemalloc.tar.bz2
	rm -rf build/jemalloc-*/
	cd build && tar xf jemalloc.tar.bz2
	cd build/jemalloc-*/ && \
		env \
			CFLAGS="-I${BUILD}/include -O3 -fdata-sections -ffunction-sections" \
			CXXFLAGS="-I${BUILD}/include -O3 -fdata-sections -ffunction-sections" \
			LDFLAGS="-L${BUILD}/lib" \
		./configure \
			--prefix="${BUILD}" --libdir="${BUILD}/lib" \
			--includedir="${BUILD}/include" --bindir="${BUILD}/bin" \
			--enable-static --disable-shared \
			--disable-stats --disable-libdl --disable-doc
	+${MAKE} -C build/jemalloc-*/
	+${MAKE} -C build/jemalloc-*/ install
	date >$@

build/gmp.done: build/gmp.tar.xz
	rm -rf build/gmp-*/
	cd build && tar xf gmp.tar.xz
	cd build/gmp-*/ && \
		env \
			CFLAGS="-I${BUILD}/include -O3 -fdata-sections -ffunction-sections" \
			CXXFLAGS="-I${BUILD}/include -O3 -fdata-sections -ffunction-sections" \
			LDFLAGS="-L${BUILD}/lib" \
		./configure \
			--prefix="${BUILD}" --libdir="${BUILD}/lib" \
			--includedir="${BUILD}/include" --bindir="${BUILD}/bin" \
			--enable-static --disable-shared --enable-cxx
	+${MAKE} -C build/gmp-*/
	+${MAKE} -C build/gmp-*/ install
	date >$@

build/mpfr.done: build/mpfr.tar.xz build/gmp.done
	rm -rf build/mpfr-*/
	cd build && tar xf mpfr.tar.xz
	cd build/mpfr-*/ && \
		env \
			CFLAGS="-I${BUILD}/include -O3 -fdata-sections -ffunction-sections" \
			CXXFLAGS="-I${BUILD}/include -O3 -fdata-sections -ffunction-sections" \
			LDFLAGS="-L${BUILD}/lib" \
		./configure \
			--prefix="${BUILD}" --libdir="${BUILD}/lib" \
			--includedir="${BUILD}/include" --bindir="${BUILD}/bin" \
			--enable-static --disable-shared --enable-thread-safe
	+${MAKE} -C build/mpfr-*/
	+${MAKE} -C build/mpfr-*/ install
	date >$@

build/flint.done: build/flint.tar.gz build/gmp.done build/mpfr.done
	rm -rf build/flint-*/
	cd build && tar xf flint.tar.gz
	cd build/flint-*/ && \
		./configure \
			--prefix="${BUILD}" --enable-static --disable-shared \
			CFLAGS="-ansi -pedantic -Wall -O3 -funroll-loops -g -I${BUILD}/include -fdata-sections -ffunction-sections"
	+${MAKE} -C build/flint-*/
	+${MAKE} -C build/flint-*/ install
	date >$@

build/zlib.done: build/zlib.tar.xz
	rm -rf build/zlib-*/
	cd build && tar xf zlib.tar.xz
	cd build/zlib-*/ && \
		env \
			CFLAGS="-O3 -fdata-sections -ffunction-sections" \
		./configure \
			--prefix="${BUILD}" --static
	+${MAKE} -C build/zlib-*/
	+${MAKE} -C build/zlib-*/ install
	date >$@

build/firefly.done: build/firefly.tar.bz2 build/flint.done build/zlib.done
	rm -rf build/firefly-*/
	cd build && tar xf firefly.tar.bz2
	sed -i.bak \
		-e '/ff_insert/d' \
		-e 's/FireFly_static FireFly_shared/FireFly_static/' \
		-e '/FireFly_shared/d' \
		-e '/example/d' \
		build/firefly-*/CMakeLists.txt
	cd build/firefly-*/ && \
		env \
			CFLAGS="-I${BUILD}/include -O3 -fdata-sections -ffunction-sections" \
			CXXFLAGS="-I${BUILD}/include -O3 -fdata-sections -ffunction-sections" \
			LDFLAGS="-L${BUILD}/lib" \
		cmake . \
			-DCMAKE_INSTALL_PREFIX="${BUILD}" -DWITH_FLINT=true \
			-DFLINT_INCLUDE_DIR="${BUILD}/include" -DFLINT_LIBRARY="xxx" \
			-DGMP_INCLUDE_DIRS="${BUILD}/include" -DGMP_LIBRARIES="xxx" \
			-DZLIB_INCLUDE_DIR="${BUILD}/include" -DZLIB_LIBRARY="xxx"
	+${MAKE} -C build/firefly-*/ VERBOSE=1
	+${MAKE} -C build/firefly-*/ install
	date >$@

build/ratracer.o: ratracer.cpp ratracer.h ratbox.h primes.h build/firefly.done
	${CXX} ${XCFLAGS} -c -o $@ ratracer.cpp

build/ratracer: build/ratracer.o build/jemalloc.done
	${CXX} ${XCFLAGS} -o $@ build/ratracer.o ${XLDFLAGS}

build/ratracer.static: build/ratracer.o build/jemalloc.done
	${CXX} ${XCFLAGS} -static -o $@ build/ratracer.o ${XLDFLAGS}

ratracer: build/ratracer
	strip -o $@ build/ratracer

ratracer.static: build/ratracer.static
	strip -o $@ build/ratracer.static
