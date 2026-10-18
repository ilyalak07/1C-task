#!/bin/sh
# Три прогона из условия через локального судью

cd "$(dirname "$0")/.." || exit 1
BIN=${1:-./build/oldmap}
status=0

check() {  # карта, обвалившиеся ходы, ожидаемый ответ, название
    got=$(python3 tests/judge.py "$1" --broken "$2" -- "$BIN" 2>/dev/null)
    if [ "$got" = "$3" ]; then
        echo "OK    $4: $got"
    else
        echo "FAIL  $4: получено '$got', ожидалось '$3'"
        status=1
    fi
}

check examples/statement.map           х2       "RESULT 42 1 38" "прогон 1 (х2 обвалился)"
check examples/statement.map           х1       "RESULT 42 1 38" "прогон 2 (х1 обвалился)"
check examples/statement_expensive.map х1,х2,х3 "RESULT 0 0 0"   "прогон 3 (day_cost 50, обвалилось всё)"
exit $status
