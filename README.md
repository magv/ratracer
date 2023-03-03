# RATRACER

Rational Tracer (*ratracer*) is a C++ library and a toolbox for
reconstructing rational expressions via modular arithmetics.

The idea of modular methods is to execute your algorithm --
whatever it might be -- many times with inputs being numbers
modulo a prime, and then to reconstruct the output as rational
functions from the results of these many runs. Because on each
run the sequence of the steps should be the same (only the input
values should be different), it makes sense to run the algorithm
just once, recording a trace of every operation performed on the
modular numbers, and then instead of re-running the algorithm
itself many times, re-run only the trace -- a much faster and
simpler thing to do.

*Ratracer* implements this idea via the `ratracer.h` library
that can record traces of arbitrary rational operations. The
`ratracer` tool is then able to read, optimize, and ultimately
reconstruct these traces.

The `ratracer` tool additionally contains means to:
- trace arbitrary arithmentic expressions from textual files;
- trace the solutions of systems of linear equations;
- expand any trace into a series.

For more information please refer to the paper at [arXiv:2211.03572].
If you are citing *ratracer*, consider also citing [FireFly]:
it provides the reconstruction routines that *ratracer* relies
on.

[arXiv:2211.03572]: https://arxiv.org/abs/2211.03572

**Ratracer is a work in progress, stability of the programming
interface is intended, but not guaranteed.**

# BUILDING

To use the *ratracer* C++ library just include the `ratracer.h`
file; there is no build step. The resulting program will need to
be linked with the [Flint] library, as well as its dependencies:
[GMP], [MPFR], and [zlib].

To build the `ratracer` tool, just run:

    make

If there are newer C++ compilers available on the system, then
use the `CC` and `CXX` variables to specify them. For example
for GCC 11 one might use:

    make CC=gcc-11 CXX=g++-11

If you have `N` cores and want to speed up the build, add the
`-j N` argument to the `make` invocation.

The `ratracer` tool itself depends on [FireFly], [Flint],
[GMP], [MPFR], [zlib], and [Jemalloc] libraries. These will be
automatically downloaded and compiled.

[gmp]: https://gmplib.org/
[mpfr]: https://mpfr.loria.fr/
[flint]: https://flintlib.org/
[jemalloc]: http://jemalloc.net/
[firefly]: https://gitlab.com/firefly-library/firefly
[zlib]: https://zlib.net/

## BUILDING THE DOCUMENTATION

To build the documentation you will need an installation of
LaTeX, and [LyX] version 2.3 or newer. With these set up, run:

    make doc/ratracer.pdf

Note that an older version of `ratracer.pdf` is available at
[arXiv:2211.03572].

[lyx]: https://www.lyx.org/

# LIBRARY USAGE

The library `ratracer.h` can be used like so:

    #include "ratracer.h"

    int
    main()
    {
        Tracer tr = tracer_init();
        Value x = tr.var(tr.input("x"));
        Value y = tr.var(tr.input("y"));
        Value expr = tr.add(tr.pow(x, 2), tr.mulint(y, 3));
        tr.add_output(expr, "expr");
        tr.save("example.trace.gz");
        return 0;
    }

Please refer to `ratracer.h` itself for further documentation.

The example can be compiled as:

    c++ -o example example.cpp -lflint -lmpfr -lgmp

The resulting trace file, `example.trace.gz`, can then be operated
on using the `ratracer` tool, e.g.:

    ratracer load-trace example.trace.gz reconstruct

# MANUAL

## NAME

Rational Tracer Toolbox (`ratracer`) -- a tool for reconstructing
rational expressions via modular arithmetics.

## SYNOPSYS

`ratracer` **command** *args* ... **command** *args* ...

## DESCRIPTION

`ratracer` contains tools to simplify complicated rational
expressions into a normal form. It can simplify
- arithmetic expressions provided as text files;
- arbitrary computations provided as trace files;
- solutions of linear equation systems.

`ratracer` works by tracing the evaluation of a given expression,
and replaying the trace using modular arithmetics inside
a rational reconstruction algorithm. It contains tools to
record, save, inspect, and optimize the traces.

## EXAMPLE

To simplify a single expression:

    echo '2*y/(x^2-y^2) + 1/(x+y) + 1/(x-y)' >expression.txt
    ratracer trace-expression expression.txt reconstruct
    [...]
    expression.txt =
      (1)/(1/2*x+(-1/2)*y);

To solve a linear system of equations:

    ratracer \
        load-equations equations.list \
        solve-equations \
        choose-equation-outputs --maxr=7 --maxs=1 \
        optimize \
        finalize \
        reconstruct

## COMMANDS

* **load-trace** *filename*

  Load the given trace.

  Note that in this and all other commands files are
  automatically compressed and decompressed based on their
  filename. If the filename ends with `.gz`, `.bz2`,
  `.xz`, or `.zst`, then `gzip`, `bzip2`, `xz`,
  or `zstd` commands will be used to read or write them.

  The recommended compression format for trace files is
  `.zst`, because it is the fastest while still providing
  considerable compression. Please install the `zstd`
  tool to use it.

* **save-trace** *filename*

  Save the current trace to a file.

* **show**

  Show a short summary of the current trace.

* **list-inputs** [`--to`=*filename*]

  Print the full list of inputs of the current trace.

* **list-outputs** [`--to`=*filename*]

  Print the full list of outputs of the current trace, one
  line per output of the form `n name`, where `n` is
  the output's sequential number (starting from 0).

* **rename-outputs** *filename*

  Read a list of output names from a file in the same
  format as in **list-outputs**, and rename each listed
  output to the corresponding name.

* **stat**

  Collect and show the current code statistics.

* **disasm** [`--to`=*filename*]

  Print a disassembly of the current trace.

* **measure**

  Measure the evaluation speed of the current trace.

* **set** *name* *expression*

  Set the given variable to the given expression in
  the further traces created by **trace-expression**,
  **load-equations**, or loaded via **load-trace**.

* **unset** *name*

  Remove the mapping specified by **set**.

* **trace-expression** *filename*

  Load a rational expression from a file and trace its
  evaluation.

* **keep-outputs** *filename*

  Read a list of output name patterns from a file, one
  pattern per line; keep all the outputs that match any
  of these patterns, and erase all the others.

  The pattern syntax is simple: `*` stands for any
  sequence of any characters, all other characters stand
  for themselves.

* **drop-outputs** *filename*

  Read a list of output names patterns from a file, one
  pattern per line; erase all outputs contained in the list.
  The pattern syntax is the same as in **keep-outputs**.

* **optimize**

  Optimize the current trace by propagating constants,
  merging duplicate expressions, and erasing dead code.

* **finalize**

  Convert the (not yet finalized) code into a final low-level
  representation that is smaller, and has drastically
  lower memory usage. Automatically eliminate the dead
  code while finalizing.

* **unfinalize**

  The reverse of **finalize** (i.e. convert low-level code
  into high-level code), except that the eliminated code
  is not brought back.

* **divide-by** *filename*

  Load factors from a file in the same format as in
  **reconstruct**, and divide corresponding outputs by
  them.

* **reconstruct** [`--to`=*filename*] [`--multiply-by`=*filename*] [`--threads`=*n*] [`--inmem`] [`--factor-scan`] [`--shift-scan`] [`--bunches`=*n*]

  Reconstruct the rational form of the current trace using
  the FireFly library.

  If the `--multiply-by` option is provided, load the
  factors from the given file, and multiply the outputs by
  then after the reconstruction is over. Note that pure
  textual substitution is assumed here, so syntax is not
  checked, and substitutions established via **set** are
  not applied.

  If the `--inmem` flag is set, load the whole code
  into memory during reconstruction; this increases the
  performance especially with many threads, but comes at
  the price of higher memory usage.

  This command uses the FireFly library for the reconstruction.
  Flags `--factor-scan` and `--shift-scan` enable
  enable FireFly's factor scan and/or shift scan (which are
  normally recommended); `--bunches` sets its maximal
  bunch size.

* **reconstruct0** [`--to`=*filename*] [`--multiply-by`=*filename*] [`--threads`=*n*]

  Same as **reconstruct**, but assumes that there are 0
  input variables needed, and is therefore faster.

  This command does not use the FireFly library. The code
  is always loaded into memory (as with the `--inmem`
  option of **reconstruct**).

* **evaluate**

  Evaluate the trace in terms of rational numbers.

  Note that all the variables must have been previously
  substituted, e.g. using the **set** command.

* **define-family** *name* [`--indices`=*n*]

  Predefine an indexed family with the given number of
  indices used in the equation parsing. This is only needed
  to guarantee the ordering of the families, otherwise
  they are auto-detected from the equation files.

  Up to 64 different families are currently supported,
  each with up to 16 indices, and the total sum of the
  absolute index values of at most 16.

* **load-equations** *file.eqns*

  Load linear equations from the given file in Kira format,
  tracing the expressions.

* **drop-equations**

  Forget all current equations and families.

* **solve-equations**

  Solve all the currently loaded equations by Gaussian
  elimination, tracing the process.

  Do not forget to **choose-equation-outputs** after this.

* **choose-equation-outputs** [`--family`=*name*] [`--maxr`=*n*] [`--maxs`=*n*] [`--maxd`=*n*]

  Mark the equations defining the specified integrals
  as the outputs, so they could be later reconstructed.

  That is, for each selected equation of the form
  $- I_0 + \sum_i I_i C_i = 0$, add each of the coefficients
  $C_i$ as an output with the name $CO[I_0,I_i]$.

  This command will fail if the equations are not in the
  fully reduced form (i.e. after **solve-equations**).

  The equations are filtered by the family name, maximal
  sum of integral's positive powers (`--maxr`), maximal
  sum of negative powers (`--maxs`), and/or maximal sum
  of powers above one (`--maxd`).

* **show-equation-masters** [`--family`=*name*] [`--maxr`=*n*] [`--maxs`=*n*] [`--maxd`=*n*]

  List the unreduced items of the equations filtered the
  same as in **choose-equation-outputs**.

* **dump-equations** [`--to`=*filename*]

  Dump the current list of equations with numeric coefficients.
  This should only be needed for debugging.

* **to-series** *varname* *maxorder*

  Re-run the current trace treating each value as a series
  in the given variable, and splitting each output into
  separate outputs per term in the series.

  The given variable is eliminated from the trace as a
  result. The variable mapping is also reset.

* **sh** *command*

  Run the given shell command.

* **help**

  Show a help message and quit.

## ENVIRONMENT

* `TMP`

  `ratracer` always keeps the current trace in one or more
  temporary files in this directory. The files themselves
  have no names, so they will not be visible to `ls`,
  but they will take disk space.

## AUTHORS

Vitaly Magerya
