#!/bin/sh

echo '\begin{itemize}'
cat |
    sed -n '{/COMMANDS/,/AUTHORS/p;/AUTHORS/d}' |
    sed -E '/AUTHORS/d;/COMMANDS/d' |
    sed -E 's/^    (Cm\{.*)/\\item \1\n/g' |
    sed -E 's/([A-Z][a-z]\{)/\\man\1/g' |
    sed -E 's/--/-{}-/g' |
    sed -E 's/^  *//' |
    sed -E 's/(FireFly|Kira)/\\noun{\1}/g' |
    cat
echo '\end{itemize}'
