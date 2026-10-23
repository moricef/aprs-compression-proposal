CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Werror -Ireference
SRC = reference/aprs_ppm.c model/aprs_ppm_model_v1.c

build/ppm_tool: test/ppm_tool.c $(SRC) reference/aprs_ppm.h reference/aprs_ppm_model.h
	@mkdir -p build
	$(CC) $(CFLAGS) test/ppm_tool.c $(SRC) -o $@

test: build/ppm_tool
	build/ppm_tool check test/vectors_v1.txt

vectors: build/ppm_tool
	build/ppm_tool gen < test/synthetic_packets.txt > test/vectors_v1.txt

clean:
	rm -rf build
.PHONY: test vectors clean
