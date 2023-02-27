#!/bin/sh

echo '\begin{itemize}'
cat |
    sed -n '/COMMANDS/,/ENVIRONMENT/p' |
    sed -E '/ENVIRONMENT/d;/COMMANDS/d' |
    sed -E ':a;/^ *[^| ].*\\$/{N;s/.\n *//;ba};p;d' |
    sed -E 's/^    (Cm\{.*)/\\item Def\1\n/g' |
    sed -E 's/((Def)?[A-Z][a-z]\{)/\\man\1/g' |
    sed -E 's/--/-{}-/g' |
    sed -E 's/^  *//' |
    sed -E 's/(FireFly|Kira)/\\noun{\1}/g' |
    cat
echo '\end{itemize}'
