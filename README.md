# RATRACER

Rational Tracer (*ratracer*) is a C++ library and a toolbox for
reconstructing rational expressions via modular arithmetics.

The idea of modular methods is to execute your algorithm --
whatever it might be -- many times with inputs being numbers
modulo a prime, and then to reconstruct the output as rational
functions from the results of these many runs. Because on each
run the sequence of the steps should be the same (only the input
values are different), it makes sense to run your algorithm
just once recording a trace of every operation performed on the
modular numbers, and then instead of re-running the algorithm
itself many times, re-run just the trace -- a much faster and
simpler thing to do.

*Ratracer* implements this idea via the `ratracer.h` library
that can record traces of arbitrary rational operations. The
`ratracer` tool is then able to read, optimize, and ultimately
reconstruct these traces.

The *ratracer* tool additonally contains means to:
- trace arbitrary arithmentic expressions from textual files;
- trace the solutions of systems of linear equations.

**Ratracer is a work in progress, stability is not yet guaranteed.**

# BUILDING

To use the *ratracer* library just include the `ratracer.h` file;
there is no build step.

To build the *ratracer* tool, first install [GMP], [MPFR],
[Flint], and [FireFly] libraries (`bb-per-thread` branch), then
adjust the `FIREFLY_CFLAGS` and `FIREFLY_LDFLAGS` variables in
the Makefile, and finally run:

    make

[gmp]: https://gmplib.org/
[mpfr]: https://mpfr.loria.fr/
[flint]: https://flintlib.org/
[firefly]: https://gitlab.com/firefly-library/firefly

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
    ratracer trace-expression expression.txt optimize reconstruct
    [...]
    expression.txt =
      (1)/(1/2*x+(-1/2)*y);

To solve a linear system of equations:

    ratracer \
        load-equations equations.list \
        solve-equations \
        choose-equation-outputs --maxr=7 --maxs=1 \
        optimize \
        reconstruct

## COMMANDS

* **load-trace** *file.trace*

  Load the given trace.

* **save-trace** *file.trace*

  Save the current trace to a file.

* **show**

  Print a short summary of the current trace.

* **disasm**

  Print a disassembly of the current trace.

* **toC**

  Print a C++ source file of an evaluation library
  corresponding to the current trace. The library can then
  be compiled with e.g.

      c++ -shared -fPIC -Os -o file.so file.cpp

* **measure**

  Measure the evaluation speed of the current trace.

* **set** *name* *expression*

  Set the given variable to the given expression in
  the further traces created by **trace-expression**,
  **load-equations**, or loaded via **load-trace**.

* **unset** *name*

  Remove the mapping specified by **set**.

* **load-trace** *file.trace*

  Load the given trace.

* **trace-expression** *filename*

  Load a rational expression from a file and trace its
  evaluation.

* **optimize**

  Optimize the current trace.

* **reconstruct** [`--to`=*filename*] [`--threads`=*n*] [`--factor-scan`] [`--shift-scan`]

  Reconstruct the rational form of the current trace using
  the FireFly library. Optionally enable FireFly's factor
  scan and/or shift scan.

* **define-family** *name* [`--indices`=*n*]

  Predefine an indexed family with the given number of
  indices used in the equation parsing. This is only needed
  to guarantee the ordering of the families, otherwise
  they are auto-detected from the equation files.

* **load-equations** *file.eqns*

  Load the equations from the given file, tracing the
  expressions.

* **solve-equations**

  Solve all the currently loaded equations by gaussian
  elimination, tracing the process.

  Don't foget to **choose-equation-outputs** after this.

* **choose-equation-outputs** [`--family`=*name*] [`--maxr`=*n*] [`--maxs`=*n*] [`--maxd`=*n*]

  Mark the equations containing the specified integrals
  as the outputs, so they could be later reconstructed.

  This command will fail if the equations are not in the
  fully reduced form (i.e. after **solve-equations**).

* **show-equation-masters** [`--family`=*name*] [`--maxr`=*n*] [`--maxs`=*n*] [`--maxd`=*n*]

  List the unreduced items of the equations filtered by
  the given family/max-r/max-s/max-d values.

* **dump-equations** [`--to`=*filename*]

  Dump the current list of equations with numeric coefficients.
  This should only be needed for debugging.

* **sh** *command*

  Run the given shell command.

* **help**

  Show this help message and quit.

## AUTHORS

Vitaly Magerya
