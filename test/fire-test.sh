#!/bin/bash
set -u


run_test_valgrind() {
	local input="$1"
	local output="$2"
	local outputf="${output//#/0}"
	local ref="${outputf//fire-output/fire-ref}"
	local vars="$3"
	local lhs="$4"
	local rhs="$5"

	printf 'Running for %-40s : ' "$input"

	if ! OUT=$(valgrind --leak-check=full --errors-for-leak-kinds=definite --error-exitcode=1 \
		../bin/ibppp --fire-table "$input" --form-fill "$output" \
		--cpus 1 --vars "$vars" --f-lhs "$lhs" --f-rhs "$rhs" 2>&1); then

		echo "FAILED (ibppp)"
		echo "$OUT"
		echo ""
		return 1
	fi

	if ! cmp -s <(gunzip -c "$outputf") <(gunzip -c "$ref"); then
		echo "FAILED (bad output)"
		echo "$OUT"
		echo ""
		return 1
	fi

	echo "OK"
	return 0
}

err=0
mkdir fire-output

# This set of tests runs under valgrind
run_test_valgrind fire-tables/doublebox.tables.m.gz fire-output/fill-doublebox.#.h.gz d,s,t db midb || err=1
run_test_valgrind fire-tables/nbox2w.6-6.tables.m.gz fire-output/fill-nbox2w.6-6.#.h.gz d,m,u,v,w nb minb || err=1
run_test_valgrind fire-tables/pentabox.14-26.tables.m.gz fire-output/fill-pentabox.14-26.#.h.gz d,s23,s34,s45,s51 pb mipb || err=1
run_test_valgrind fire-tables/v2.tables.m.gz fire-output/fill-v2.#.h.gz d v miv || err=1

rm -rf fire-output
exit "$err"

