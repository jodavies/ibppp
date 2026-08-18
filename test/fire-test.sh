#!/bin/bash
set -u

# Put the results (which may be split non-deterministically among a file per thread)
# into a canonical form for comparison with reference files.
format() {
	cat | sed ':a;N;$!ba;s/\n\t/ /g' | sed '/^$/d' | sort
}

run_test_valgrind() {
	local input="$1"
	local outputbase="$2"
	local outputmask="fire-output/${outputbase}#.h.gz"
	local ref="fire-ref/${outputbase}0.h.gz"
	local cpus="$3"
	local vars="$4"
	local lhs="$5"
	local rhs="$6"

	printf 'Running for %-40s : (%s cpus) : ' "$input" "$cpus"

	if ! OUT=$(valgrind --leak-check=full --errors-for-leak-kinds=definite --error-exitcode=1 \
		../bin/ibppp --fire-table "$input" --form-fill "$outputmask" \
		--cpus "$cpus" --vars "$vars" --f-lhs "$lhs" --f-rhs "$rhs" 2>&1); then

		echo "FAILED (ibppp)"
		echo "$OUT"
		echo ""
		return 1
	fi

	if ! cmp -s <(gunzip -c fire-output/"${outputbase}"* | format) <(gunzip -c "$ref" | format); then
		echo "FAILED (bad output)"
		echo "$OUT"
		echo ""
		return 1
	fi

	echo "OK"
	return 0
}

err=0
mkdir -p fire-output
rm fire-output/*

# This set of tests runs under valgrind
run_test_valgrind fire-tables/doublebox.tables.m.gz      fill-doublebox.      1 d,s,t             db midb || err=1
run_test_valgrind fire-tables/nbox2w.6-6.tables.m.gz     fill-nbox2w.6-6.     1 d,m,u,v,w         nb minb || err=1
run_test_valgrind fire-tables/pentabox.14-26.tables.m.gz fill-pentabox.14-26. 1 d,s23,s34,s45,s51 pb mipb || err=1
run_test_valgrind fire-tables/v2.tables.m.gz             fill-v2.             1 d                 v  miv  || err=1

run_test_valgrind fire-tables/doublebox.tables.m.gz      fill-doublebox.      4 d,s,t             db midb || err=1
run_test_valgrind fire-tables/nbox2w.6-6.tables.m.gz     fill-nbox2w.6-6.     4 d,m,u,v,w         nb minb || err=1
run_test_valgrind fire-tables/pentabox.14-26.tables.m.gz fill-pentabox.14-26. 4 d,s23,s34,s45,s51 pb mipb || err=1
run_test_valgrind fire-tables/v2.tables.m.gz             fill-v2.             4 d                 v  miv  || err=1

rm -rf fire-output
exit "$err"

