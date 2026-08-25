#!/bin/bash
set -u

# Put the results (which may be split non-deterministically among a file per thread)
# into a canonical form for comparison with reference files.
format() {
	cat | sed ':a;N;$!ba;s/\n\t/ /g' | sed '/^$/d' | sort
}

run_test_valgrind() {
	local mode="$1"
	local input="$2"
	local outputbase="$3"
	local outputmask="output/${outputbase}#.h.gz"
	local ref="ref/${outputbase}0.h.gz"
	local cpus="$4"
	local vars="$5"
	local lhs="$6"
	local rhs="$7"
	local ep_expand="${8:-}"

	if [[ -n "$ep_expand" ]]; then
		local extra_args=(--ep-expand "$ep_expand")
	else
		local extra_args=()
	fi


	printf 'Running for %-40s : (ep^%-2s) (%s cpus) : ' "$input" "$ep_expand" "$cpus"

	if ! OUT=$(valgrind --leak-check=full --errors-for-leak-kinds=definite --error-exitcode=1 \
		../bin/ibppp --"$mode"-table "$input" --form-fill "$outputmask" \
		--cpus "$cpus" --vars "$vars" --f-lhs "$lhs" --f-rhs "$rhs" "${extra_args[@]}" 2>&1); then

		echo "FAILED (ibppp)"
		echo "$OUT"
		echo ""
		return 1
	fi

	if ! cmp -s <(gunzip -c output/"${outputbase}"* | format) <(gunzip -c "$ref" | format); then
		echo "FAILED (bad output)"
		echo "$OUT"
		echo ""
		return 1
	fi

	echo "OK"
	return 0
}

err=0
mkdir -p output
rm -f output/*

# This set of tests runs under valgrind
for cpu in 1 4; do
	for ep in "" 0 1 5; do
		run_test_valgrind fire tables/fire-doublebox.tables.m.gz      fill-doublebox.ep"$ep".             "$cpu" d,s,t             db midb "$ep" || err=1
		run_test_valgrind fire tables/fire-nbox2w.6-6.tables.m.gz     fill-nbox2w.6-6.ep"$ep".            "$cpu" d,m,u,v,w         nb minb "$ep" || err=1
		run_test_valgrind fire tables/fire-pentabox.14-26.tables.m.gz fill-pentabox.14-26.ep"$ep".        "$cpu" d,s23,s34,s45,s51 pb mipb "$ep" || err=1
		run_test_valgrind fire tables/fire-v2.tables.m.gz             fill-v2.ep"$ep".                    "$cpu" d                 v  miv  "$ep" || err=1

		run_test_valgrind kira tables/kira-box.m.gz                   fill-box.ep"$ep".                   "$cpu" d,s,t             "" mi   "$ep" || err=1
		run_test_valgrind kira tables/kira-box_firefly.m.gz           fill-box_firefly.ep"$ep".           "$cpu" d,s,t             "" mi   "$ep" || err=1
		run_test_valgrind kira tables/kira-topo7massless.m.gz         fill-topo7massless.ep"$ep".         "$cpu" d,t               "" mi   "$ep" || err=1
		run_test_valgrind kira tables/kira-topo7massless_firefly.m.gz fill-topo7massless_firefly.ep"$ep". "$cpu" d,t               "" mi   "$ep" || err=1
	done
done

rm -rf output
exit "$err"

