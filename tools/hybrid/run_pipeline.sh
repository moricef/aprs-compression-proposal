#!/bin/bash
# End to end: prep -> train one table per layout -> build -> run -> summary.
#   INPUTS="cap1.txt cap2.txt.gz" [MINCOUNT=128] [WORK=work] [PREP_ARGS="--all-rf --dest-prefix ''"] ./run_pipeline.sh
# Needs a C compiler and python3. Takes a few minutes on ~100,000 packets; the largest step is the harness (one run per layout).
set -e
here="$(cd "$(dirname "$0")" && pwd)"; repo="$here/../.."
: "${INPUTS:?set INPUTS to the capture files}"; MINCOUNT="${MINCOUNT:-128}"; WORK="${WORK:-$here/work}"; mkdir -p "$WORK"
echo "== prep"; eval python3 "$here/prep.py" "$WORK/test.tsv" $INPUTS ${PREP_ARGS:-}
for m in whole nopath info; do
  echo "== train $m"
  eval python3 "$here/train_layout_model.py" --lora $INPUTS --mode $m --min-count $MINCOUNT --holdout 0.3 --version 2 --out "$WORK/model_$m.c" ${TRAIN_ARGS:-}
  cc -std=c99 -O2 -w -I"$repo/reference" "$here/hybrid_layouts.c" "$repo/reference/aprs_ppm.c" "$WORK/model_$m.c" -lm -o "$WORK/bin_$m"
done
echo "== run (direct N=1 2 4, then one relay hop with and without RXT)"
pids=()
i=0; for m in whole nopath info; do i=$((i+1)); mask=$((1 << (i-1)))
  ( LAYOUTS=$mask "$WORK/bin_$m" "$WORK/test.tsv" 1 2 4 > "$WORK/out_$m.txt" ) & pids+=($!)
  ( RXT=1 LAYOUTS=$mask "$WORK/bin_$m" "$WORK/test.tsv" 2 4 > "$WORK/out_${m}_rxt.txt" ) & pids+=($!)
  ( RXT=1 RXTFULL=1 LAYOUTS=$mask "$WORK/bin_$m" "$WORK/test.tsv" 2 4 > "$WORK/out_${m}_rxtfull.txt" ) & pids+=($!)
done
for p in "${pids[@]}"; do wait $p; done
echo; echo "######## no RXT"; python3 "$here/summarize.py" "$WORK/out_whole.txt" "$WORK/out_nopath.txt" "$WORK/out_info.txt"
echo; echo "######## RXT on every relayed frame (relayed rows only apply)"; python3 "$here/summarize.py" "$WORK/out_whole_rxt.txt" "$WORK/out_nopath_rxt.txt" "$WORK/out_info_rxt.txt" | sed -n '1p;/relayed/,$p'
echo; echo "######## RXT on full frames only"; python3 "$here/summarize.py" "$WORK/out_whole_rxtfull.txt" "$WORK/out_nopath_rxtfull.txt" "$WORK/out_info_rxtfull.txt" | sed -n '1p;/relayed/,$p'
echo; echo "Table sizes (trainer flash estimates):"; for m in whole nopath info; do echo "  $m: $(sed -n 2p "$WORK/model_$m.c" | cut -c4-)"; done
