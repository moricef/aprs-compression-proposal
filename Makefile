CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Werror -Ireference
SRC = reference/aprs_ppm.c model/aprs_ppm_model_v1.c

build/ppm_tool: test/ppm_tool.c $(SRC) reference/aprs_ppm.h reference/aprs_ppm_model.h
	@mkdir -p build
	$(CC) $(CFLAGS) test/ppm_tool.c $(SRC) -o $@

test: build/ppm_tool
	build/ppm_tool check test/vectors_v1.txt
	build/ppm_tool check-primed test/vectors_primed_v1.txt
	build/ppm_tool collide test/vectors_primed_v1.txt

build/trunc_ref: tools/integrity/trunc_ref.c $(SRC) reference/aprs_ppm.h reference/aprs_ppm_model.h
	@mkdir -p build
	$(CC) $(CFLAGS) tools/integrity/trunc_ref.c model/aprs_ppm_model_v1.c -o $@

build/primed_c_test: tools/integrity/primed_c_test.c $(SRC) reference/aprs_ppm.h reference/aprs_ppm_model.h
	@mkdir -p build
	$(CC) $(CFLAGS) tools/integrity/primed_c_test.c model/aprs_ppm_model_v1.c -o $@

# truncation and corruption of full frames with check C, on fabricated packets (see tools/integrity/README.txt)
integrity: build/trunc_ref
	build/trunc_ref

build/lacf_ending: tools/integrity/lacf_ending.c $(SRC) reference/aprs_ppm.h reference/aprs_ppm_model.h
	@mkdir -p build
	$(CC) $(CFLAGS) tools/integrity/lacf_ending.c model/aprs_ppm_model_v1.c -o $@

# the LACF ending and the receiver's re-encode check (spec/10), checked against its vectors and on fabricated packets
lacf-ending: build/lacf_ending
	build/lacf_ending check test/vectors_lacf_v1.txt
	build/lacf_ending

build/fast_freq_test: tools/integrity/fast_freq_test.c tools/integrity/fast_freq.h $(SRC) reference/aprs_ppm.h reference/aprs_ppm_model.h
	@mkdir -p build
	$(CC) $(CFLAGS) tools/integrity/fast_freq_test.c model/aprs_ppm_model_v1.c -o $@

# the faster build_freq (tools/integrity/fast_freq.h) must equal the reference's output; needs a text file: make fast-freq TEXTS=texts.txt (fabricated: test/synthetic_packets.txt)
fast-freq: build/fast_freq_test
	build/fast_freq_test $(or $(TEXTS),test/synthetic_packets.txt)

lacf-vectors: build/lacf_ending
	cat test/synthetic_packets.txt test/lacf_edge_packets.txt | build/lacf_ending gen > test/vectors_lacf_v1.txt

vectors: build/ppm_tool
	build/ppm_tool gen < test/synthetic_packets.txt > test/vectors_v1.txt
	build/ppm_tool gen-primed < test/synthetic_primed_pairs.txt > test/vectors_primed_v1.txt

clean:
	rm -rf build
.PHONY: test vectors clean integrity lacf-ending lacf-vectors fast-freq
