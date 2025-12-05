XCXXFLAGS=\
	-Ibuild/include \
	-O3 -g -std=c++14 -fopenmp \
	-Wall -Wextra -Wfatal-errors \
	-pipe -fno-omit-frame-pointer \
	-fdata-sections -ffunction-sections -fvisibility=hidden \
	${CXXFLAGS}

XLDFLAGS=\
	-Lbuild/lib \
	${LDFLAGS} \
	-Wl,--gc-sections \
	-lfirefly -lflint -lmpfr -lgmp -lpthread -lz -ldl -ljemalloc

CC?=cc

CXX?=c++

FETCH?=wget --no-use-server-timestamps -qO

all: ratracer README.md doc/commands.tex

download: build/jemalloc.tar.bz2 build/gmp.tar.xz build/mpfr.tar.xz build/flint.tar.gz build/zlib.tar.xz build/firefly.tar.gz phony

deps: build/jemalloc.done build/gmp.done build/mpfr.done build/flint.done build/zlib.done build/firefly.done phony

docs: README.md doc/ratracer.pdf phony

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

doc/ratracer.tex: doc/ratracer.lyx
	lyx --export-to pdflatex $@.tmp $<
	sed -e '/documentclass/a\\\\pdfoutput=1' -e '/^%/d' $@.tmp >$@
	rm -f $@.tmp

doc/ratracer.pdf: doc/ratracer.tex doc/commands.tex doc/preamble.tex doc/ratracer.bib
	cd doc && latexmk -pdf ratracer.tex
	cd doc && rm -f ratracer-blx.bib *.aux *.bbl *.blg *.fdb_latexmk *.fls *.log *.out *.run.xml *.toc

build/.dir:
	mkdir -p build
	date >$@

build/jemalloc.tar.bz2: build/.dir
	${FETCH} $@ \
		"https://github.com/jemalloc/jemalloc/releases/download/5.3.0/jemalloc-5.3.0.tar.bz2" || \
		rm -f "$@"

build/gmp.tar.xz: build/.dir
	${FETCH} $@ \
		"https://gmplib.org/download/gmp/gmp-6.3.0.tar.xz" || \
		rm -f "$@"

build/mpfr.tar.xz: build/.dir
	${FETCH} $@ \
		"https://www.mpfr.org/mpfr-4.2.1/mpfr-4.2.1.tar.xz" || \
		rm -f "$@"

build/flint.tar.gz: build/.dir
	${FETCH} $@ \
		"https://flintlib.org/download/flint-3.1.2.tar.gz" || \
		rm -f "$@"

build/flintxx.tar.gz: build/.dir
	${FETCH} $@ \
		"https://github.com/flintlib/flintxx/archive/0be0a5f4da4dcf475eff00e6adbf5a728fb0153b.tar.gz" || \
		rm -f "$@"

build/zlib.tar.xz: build/.dir
	${FETCH} $@ \
		"http://zlib.net/fossils/zlib-1.3.1.tar.gz" || \
		rm -f "$@"

build/firefly.tar.gz: build/.dir
	${FETCH} $@ \
		"https://github.com/magv/firefly/archive/refs/heads/ratracer.tar.gz" || \
		rm -f "$@"

BUILD=${CURDIR}/build
DEP_CFLAGS=-I${BUILD}/include -O3 -fno-omit-frame-pointer -fdata-sections -ffunction-sections
DEP_LDFLAGS=-L${BUILD}/lib

build/jemalloc.done: build/jemalloc.tar.bz2
	rm -rf build/jemalloc-*/
	cd build && tar xf jemalloc.tar.bz2
	cd build/jemalloc-*/ && \
		env CC="${CC}" CXX="${CXX}" CFLAGS="${DEP_CFLAGS}" CXXFLAGS="${DEP_CFLAGS}" LDFLAGS="${DEP_LDFLAGS}" \
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
		env CC="${CC}" CXX="${CXX}" CFLAGS="-std=c11 ${DEP_CFLAGS}" CXXFLAGS="-std=c++11 ${DEP_CFLAGS}" LDFLAGS="${DEP_LDFLAGS}" \
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
		env CC="${CC}" CXX="${CXX}" CFLAGS="${DEP_CFLAGS}" CXXFLAGS="${DEP_CFLAGS}" LDFLAGS="${DEP_LDFLAGS}" \
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
			--prefix="${BUILD}" --libdir="${BUILD}/lib" \
			--enable-static --disable-shared --enable-arch=no \
			--with-gmp="${BUILD}" --with-mpfr="${BUILD}" \
			CC="${CC}" CXX="${CXX}" \
			CFLAGS="${DEP_CFLAGS} -g -std=c11 -O3" \
			LDFLAGS="${DEP_LDFLAGS}"
	+${MAKE} -C build/flint-*/
	+${MAKE} -C build/flint-*/ install
	date >$@

build/flintxx.done: build/flintxx.tar.gz build/flint.done build/gmp.done build/mpfr.done
	rm -rf build/flintxx-*/
	cd build && tar xf flintxx.tar.gz
	cd build/flintxx-*/ && sed --in-place -e 's,"[.][.]/flint[.]h",<flint/flint.h>,g' src/flintxx/*.h
	ln -sf flint ${BUILD}/include/flintxx
	cd build/flintxx-*/ && cp src/flintxx/*.h src/flintxx_public/*.h ${BUILD}/include/flintxx/
	date >$@

build/zlib.done: build/zlib.tar.xz
	rm -rf build/zlib-*/
	cd build && tar xf zlib.tar.xz
	cd build/zlib-*/ && \
		env CC="${CC}" CXX="${CXX}" CFLAGS="${DEP_CFLAGS}" \
		./configure \
			--prefix="${BUILD}" --static
	+${MAKE} -C build/zlib-*/
	+${MAKE} -C build/zlib-*/ install
	date >$@

build/firefly.done: build/firefly.tar.gz build/flint.done build/flintxx.done build/zlib.done
	rm -rf build/firefly-*/
	cd build && tar xf firefly.tar.gz
	cd build/firefly-*/ && \
		env CC="${CC}" CXX="${CXX}" CFLAGS="${DEP_CFLAGS}" CXXFLAGS="${DEP_CFLAGS}" LDFLAGS="${DEP_LDFLAGS}" \
		cmake . \
			-DCMAKE_INSTALL_PREFIX="${BUILD}" \
			-DCMAKE_INSTALL_LIBDIR="lib" \
			-DENABLE_STATIC=ON -DENABLE_SHARED=OFF -DENABLE_FF_INSERT=OFF -DENABLE_EXAMPLE=OFF \
			-DFLINT_INCLUDE_DIR="${BUILD}/include" -DFLINT_LIBRARY="xxx" \
			-DZLIB_INCLUDE_DIR="${BUILD}/include" -DZLIB_LIBRARY="xxx"
	+${MAKE} -C build/firefly-*/ VERBOSE=1
	+${MAKE} -C build/firefly-*/ install
	date >$@

primes.h: mkprimes
	./mkprimes >$@

build/ratracer.o: ratracer.cpp ratracer.h ratbox.h primes.h build/firefly.done
	${CXX} ${XCXXFLAGS} -c -o $@ ratracer.cpp

ratracer: build/ratracer.o build/jemalloc.done
	${CXX} ${XCXXFLAGS} -o $@ build/ratracer.o ${XLDFLAGS}

ratracer.static: build/ratracer.o build/jemalloc.done
	${CXX} ${XCXXFLAGS} -static -o $@ build/ratracer.o ${XLDFLAGS}
