# Tables used in the hybrid comparison

Three static order-3 APRS-PPM tables (model version 2), one per layout, trained on the same packets with the same settings. Only the
text each one learns differs.

| File | Layout (`--mode`) | Text it learns | Table size |
|---|---|---|---|
| `model_whole.c` | whole packet | `SRC>DST[,PATH]:INFO` | 105 KB |
| `model_hybridA_nopath.c` | hybrid A (`nopath`) | `SRC>DST:INFO` | 100 KB |
| `model_hybridB_info.c` | hybrid B (`info`) | `:INFO` | 79 KB |

## How they were built

Training data: LoRa APRS packets captured read-only from APRS-IS (24 October 2026 day capture plus earlier hourly captures), about
265,000 unique packets per mode. Stations with md5(source callsign) % 10 in buckets 0 to 2 are held out (the test set that `prep.py` writes);
the tables are trained on buckets 3 to 9 only (about 175,000 packets). The raw captures contain callsigns and are not published; each table
holds only byte-trigram context counts, with no packet text.

    python3 tools/hybrid/train_layout_model.py --lora CAPTURES... --mode whole|nopath|info \
        --min-count 128 --holdout 0.3 --version 2 --out model_<mode>.c

Rebuilt from the repo's trainer, the three files are byte-identical to the ones the results came from. With your own captures, `run_pipeline.sh`
does all of it. A min count of 128 is what fits the 16-bit offset limit for about 175,000 packets; raise it if the trainer complains.

## Using them

    cc -std=c99 -O2 -Ireference tools/hybrid/hybrid_layouts.c reference/aprs_ppm.c tools/hybrid/tables/model_whole.c -lm -o hl_whole
    LAYOUTS=1 ./hl_whole TEST.tsv 1 2 4        # whole packet; use LAYOUTS=2 with model_hybridA_nopath.c, LAYOUTS=4 with model_hybridB_info.c

Test only on stations in hash buckets 0 to 2. To exclude the same stations from another training set, split by `md5(source callsign) % 10`.
