# RELIC BLS12-381 Build Makefile
# Usage: make -f build_relic.mk

RELIC_REPO = https://github.com/relic-toolkit/relic.git
PROJECT_ROOT = $(abspath $(CURDIR)/..)
INSTALL_PREFIX = $(PROJECT_ROOT)/out/install/linux
CLONE_DIR = $(PROJECT_ROOT)/out/relic
BUILD_DIR = $(CLONE_DIR)/build

CMAKE_FLAGS = -DCMAKE_INSTALL_PREFIX=$(INSTALL_PREFIX) \
	-DCMAKE_BUILD_TYPE=Release \
	-DARCH=X64 \
	-DWSIZE=64 \
	-DRAND=UDEV \
	-DSHLIB=off \
	-DSTLIB=on \
	-DSTBIN=off \
	-DTIMER=HREAL \
	-DCHECK=off \
	-DVERBS=off \
	-DARITH=x64-asm-6l \
	-DFP_PRIME=381 \
	-DFP_PMERS=off \
	-DFP_QNRES=on \
	-DBN_PRECI=3072 \
	-DMULTI=PTHREAD \
	-DCURVE=B12-381 \
	-DFP_METHD=INTEG\;INTEG\;INTEG\;MONTY\;LOWER\;LOWER\;SLIDE \
	-DFPX_METHD=INTEG\;INTEG\;LAZYR \
	-DEP_METHD=PROJC\;JACOB\;LWNAF\;COMBS\;INTER \
	-DEP2_METHD=PROJC\;JACOB\;LWNAF\;COMBS\;INTER \
	-DPP_METHD=LAZYR\;OATEP \
	-DWITH_BN=on \
	-DWITH_DV=on \
	-DWITH_FP=on \
	-DWITH_FPX=on \
	-DWITH_EP=on \
	-DWITH_EPX=on \
	-DWITH_EP2=on \
	-DWITH_PP=on \
	-DWITH_PC=on \
	-DWITH_MD=on \
	-DWITH_FB=on

.PHONY: all clean clone configure build install verify test

all: verify

clone:
	@echo "=== Cloning RELIC ==="
	@rm -rf $(CLONE_DIR)
	@git clone $(RELIC_REPO) $(CLONE_DIR)

configure: clone
	@echo "=== Configuring RELIC ==="
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(INSTALL_PREFIX)
	@cd $(BUILD_DIR) && cmake $(CMAKE_FLAGS) $(CLONE_DIR)

build: configure
	@echo "=== Building RELIC ==="
	@cd $(BUILD_DIR) && make -j4

install: build
	@echo "=== Installing RELIC ==="
	@cd $(BUILD_DIR) && make install

verify: install
	@echo "=== Verifying RELIC Installation ==="
	@if [ -f "$(INSTALL_PREFIX)/include/relic/relic.h" ] && [ -f "$(INSTALL_PREFIX)/lib/librelic_s.a" ]; then \
		echo "RELIC successfully installed"; \
		echo "Headers: $(INSTALL_PREFIX)/include/relic/"; \
		echo "Library: $(INSTALL_PREFIX)/lib/librelic_s.a"; \
		$(MAKE) -f $(firstword $(MAKEFILE_LIST)) test; \
	else \
		echo "RELIC installation failed"; \
		echo "Checking what was actually installed:"; \
		ls -la "$(INSTALL_PREFIX)/include/" 2>/dev/null || echo "No include directory"; \
		ls -la "$(INSTALL_PREFIX)/lib/" 2>/dev/null || echo "No lib directory"; \
		exit 1; \
	fi

test:
	@echo "=== Testing RELIC Configuration ==="
	@echo '#include <relic/relic.h>' > test_relic.c
	@echo '#include <stdio.h>' >> test_relic.c
	@echo '#include <string.h>' >> test_relic.c
	@echo 'int main() {' >> test_relic.c
	@echo '    if (core_init() != RLC_OK) return 1;' >> test_relic.c
	@echo '    if (pc_param_set_any() != RLC_OK) return 1;' >> test_relic.c
	@echo '    printf("=== Testing G1 (EP) ===\\n");' >> test_relic.c
	@echo '    g1_t g1_1, g1_2;' >> test_relic.c
	@echo '    g1_null(g1_1); g1_new(g1_1);' >> test_relic.c
	@echo '    g1_null(g1_2); g1_new(g1_2);' >> test_relic.c
	@echo '    g1_get_gen(g1_1);' >> test_relic.c
	@echo '    uint8_t g1_buffer[RLC_PC_BYTES + 1];' >> test_relic.c
	@echo '    memset(g1_buffer, 0, sizeof(g1_buffer));' >> test_relic.c
	@echo '    g1_write_bin(g1_buffer, sizeof(g1_buffer), g1_1, 1);' >> test_relic.c
	@echo '    printf("G1 buffer size: %lu bytes\\n", sizeof(g1_buffer));' >> test_relic.c
	@echo '    g1_read_bin(g1_2, g1_buffer, sizeof(g1_buffer));' >> test_relic.c
	@echo '    if (g1_cmp(g1_1, g1_2) != RLC_EQ) {' >> test_relic.c
	@echo '        printf("G1 test FAILED\\n"); return 1;' >> test_relic.c
	@echo '    }' >> test_relic.c
	@echo '    printf("G1 test PASSED\\n");' >> test_relic.c
	@echo '    printf("=== Testing G2 (EP2) ===\\n");' >> test_relic.c
	@echo '    g2_t g2_1, g2_2;' >> test_relic.c
	@echo '    g2_null(g2_1); g2_new(g2_1);' >> test_relic.c
	@echo '    g2_null(g2_2); g2_new(g2_2);' >> test_relic.c
	@echo '    g2_get_gen(g2_1);' >> test_relic.c
	@echo '    printf("EP2 size: %d bytes\\n", g2_size_bin(g2_1, 0));' >> test_relic.c
	@echo '    uint8_t g2_buffer[4 * RLC_PC_BYTES + 1];' >> test_relic.c
	@echo '    memset(g2_buffer, 0, sizeof(g2_buffer));' >> test_relic.c
	@echo '    g2_write_bin(g2_buffer, sizeof(g2_buffer), g2_1, 0);' >> test_relic.c
	@echo '    printf("G2 buffer size: %lu bytes\\n", sizeof(g2_buffer));' >> test_relic.c
	@echo '    g2_read_bin(g2_2, g2_buffer, sizeof(g2_buffer));' >> test_relic.c
	@echo '    if (g2_cmp(g2_1, g2_2) != RLC_EQ) {' >> test_relic.c
	@echo '        printf("G2 comparison failed\\n"); return 1;' >> test_relic.c
	@echo '    }' >> test_relic.c
	@echo '    printf("G2 test PASSED\\n");' >> test_relic.c
	@echo '    printf("=== Testing GT (Pairing Result) ===\\n");' >> test_relic.c
	@echo '    gt_t gt_1, gt_2;' >> test_relic.c
	@echo '    gt_null(gt_1); gt_new(gt_1);' >> test_relic.c
	@echo '    gt_null(gt_2); gt_new(gt_2);' >> test_relic.c
	@echo '    pc_map(gt_1, g1_1, g2_1);' >> test_relic.c
	@echo '    uint8_t gt_buffer[12 * RLC_PC_BYTES];' >> test_relic.c
	@echo '    memset(gt_buffer, 0, sizeof(gt_buffer));' >> test_relic.c
	@echo '    gt_write_bin(gt_buffer, sizeof(gt_buffer), gt_1, 0);' >> test_relic.c
	@echo '    printf("GT buffer size: %lu bytes\\n", sizeof(gt_buffer));' >> test_relic.c
	@echo '    gt_read_bin(gt_2, gt_buffer, sizeof(gt_buffer));' >> test_relic.c
	@echo '    if (gt_cmp(gt_1, gt_2) != RLC_EQ) {' >> test_relic.c
	@echo '        printf("GT comparison failed\\n"); return 1;' >> test_relic.c
	@echo '    }' >> test_relic.c
	@echo '    printf("GT test PASSED\\n");' >> test_relic.c
	@echo '    printf("=== Testing BN (Big Number) ===\\n");' >> test_relic.c
	@echo '    bn_t bn_1, bn_2;' >> test_relic.c
	@echo '    bn_null(bn_1); bn_new(bn_1);' >> test_relic.c
	@echo '    bn_null(bn_2); bn_new(bn_2);' >> test_relic.c
	@echo '    bn_set_dig(bn_1, 12345678);' >> test_relic.c
	@echo '    uint8_t bn_buffer[RLC_FC_BYTES];' >> test_relic.c
	@echo '    memset(bn_buffer, 0, sizeof(bn_buffer));' >> test_relic.c
	@echo '    bn_write_bin(bn_buffer, sizeof(bn_buffer), bn_1);' >> test_relic.c
	@echo '    printf("BN buffer size: %lu bytes\\n", sizeof(bn_buffer));' >> test_relic.c
	@echo '    bn_read_bin(bn_2, bn_buffer, sizeof(bn_buffer));' >> test_relic.c
	@echo '    if (bn_cmp(bn_1, bn_2) != RLC_EQ) {' >> test_relic.c
	@echo '        printf("BN comparison failed\\n"); return 1;' >> test_relic.c
	@echo '    }' >> test_relic.c
	@echo '    printf("BN test PASSED\\n");' >> test_relic.c
	@echo '    printf("=== All Tests Summary ===\\n");' >> test_relic.c
	@echo '    printf("PC_BYTES: %d\\n", RLC_PC_BYTES);' >> test_relic.c
	@echo '    printf("FC_BYTES: %d\\n", RLC_FC_BYTES);' >> test_relic.c
	@echo '    printf("All serialization/deserialization tests PASSED\\n");' >> test_relic.c
	@echo '    g1_free(g1_1); g1_free(g1_2);' >> test_relic.c
	@echo '    g2_free(g2_1); g2_free(g2_2);' >> test_relic.c
	@echo '    gt_free(gt_1); gt_free(gt_2);' >> test_relic.c
	@echo '    bn_free(bn_1); bn_free(bn_2);' >> test_relic.c
	@echo '    core_clean(); return 0;' >> test_relic.c
	@echo '}' >> test_relic.c
	@gcc -I$(INSTALL_PREFIX)/include/relic -L$(INSTALL_PREFIX)/lib test_relic.c -lrelic_s -lgmp -o test_relic
	@./test_relic && echo "RELIC comprehensive test passed" || echo "RELIC comprehensive test failed"
	@rm -f test_relic test_relic.c

clean:
	@echo "=== Cleaning RELIC ==="
	@rm -rf $(CLONE_DIR)
	@rm -rf $(INSTALL_PREFIX)/include/relic*
	@rm -f $(INSTALL_PREFIX)/lib/librelic*
	@rm -f test_relic test_relic.c
