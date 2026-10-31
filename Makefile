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

vectors: build/ppm_tool
	build/ppm_tool gen < test/synthetic_packets.txt > test/vectors_v1.txt
	build/ppm_tool gen-primed < test/synthetic_primed_pairs.txt > test/vectors_primed_v1.txt

clean:
	rm -rf build
.PHONY: test vectors clean integrity
