#!/bin/bash
# usage: run.sh jobsfile  (run from v/) ; each line: name args...
cd "$(dirname "$(readlink -f "$0")")"
grep -v '^\s*$' "$1" | xargs -P 2 -L 1 bash -c 'n="$0"; if [ ! -s "$n.csv" ]; then ../sim_v "$@" > "$n.csv.tmp" && mv "$n.csv.tmp" "$n.csv"; fi; echo "done $n $(date +%T)"'
