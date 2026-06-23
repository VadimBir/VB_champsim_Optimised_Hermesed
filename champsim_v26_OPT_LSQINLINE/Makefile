app = champsim
Ext = cc
srcDir = src branch replacement prefetcher
objDir = obj
binDir = bin
inc = inc
debug = 1

.PHONY: all clean distclean install_env run_gcc run_aocc run_clang


app = champsim
srcExt = cc
srcDir = src branch replacement prefetcher
objDir = obj
binDir = bin
inc = inc
debug = 1

.PHONY: build_run all build run debug print_env index build_debug clean distclean install_env run_gcc run_aocc run_clang



L1D_Pf ?= next_line
L2C_Pf ?= next_line
LLC_Pf ?= no

ARCH ?= glc
CORES ?= 2

# BOLT binary optimizer — set BOLT=1 to enable (applies after every build automatically)
BOLT              ?= 1
BOLT_BIN          := /opt/llvm-21.1.0/bin/llvm-bolt
BOLT_PERF2BOLT    := /opt/llvm-21.1.0/bin/perf2bolt
BOLT_WARMUP       ?= 500000
BOLT_SIM          ?= 5000000
BOLT_PERF_DATA    ?= /tmp/champsim_bolt_perf.data
BOLT_FDATA        ?= /tmp/champsim_bolt.fdata


homo_traces ?= ../traces/LLM256.Pythia-70M_21M.champsimtrace.xz
WARMUP_instr ?= 2000000
SIM_instr ?= 10000000
# Generate binary name dynamically
BP        ?= perceptron
REPL      ?= lru
WIN_MODE  ?= fixed
HERMES    ?= off
L1_BYP_MODEL ?= no
L2_BYP_MODEL ?= no
L3_BYP_MODEL ?= no
ifeq ($(L1_BYP_MODEL),$(L2_BYP_MODEL))
ifeq ($(L2_BYP_MODEL),$(L3_BYP_MODEL))
BYP_TAG := ByP-$(L1_BYP_MODEL)
else
BYP_TAG := ByP-$(L1_BYP_MODEL)-$(L2_BYP_MODEL)-$(L3_BYP_MODEL)
endif
else
BYP_TAG := ByP-$(L1_BYP_MODEL)-$(L2_BYP_MODEL)-$(L3_BYP_MODEL)
endif
BIN = ./$(binDir)/$(BP)-$(L1D_Pf)-$(L2C_Pf)-$(LLC_Pf)-$(REPL)-$(CORES)core-$(BYP_TAG)-$(WIN_MODE)-$(HERMES)

# Repeat trace file per core if CORES > 1
TRACE_LIST := $(shell for i in $$(seq 1 $(CORES)); do echo -n "$(homo_traces) "; done)

CFlags = -Wall -g -O3 -std=c++17 -D_DEFAULT_SOURCE -fno-omit-frame-pointer  $(EXTRA_CFLAGS)
# CFlags = -Wall -g -O3 -std=c++17 -D_DEFAULT_SOURCE -fomit-frame-pointer $(EXTRA_CFLAGS)

# CFlags = -Wall -O0 -std=c++17 -D_DEFAULT_SOURCE -g -rdynamic -fno-omit-frame-pointer -fno-inline -fno-inline-functions -fno-optimize-sibling-calls # -fno-omit-frame-pointer -g

#build_Wno-warnings:
#	@echo "Building ChampSim binary... (ALL warnings suppressed)"
#	#CFlags += -Wno-everything -Wbackend-plugin \
#	#	./build_champsim.sh $(ARCH)_$(CORES)c $(L1D_Pf) $(L2C_Pf) $(LLC_Pf) $(CORES)
#	CFlags="$(CFlags) -Wno-everything -Wbackend-plugin" ./build_champsim.sh $(ARCH)_$(CORES)c $(L1D_Pf) $(L2C_Pf) $(LLC_Pf) $(CORES)
#	@echo "Binary built: $(BIN)"
index:
	@for f in prefetcher/*.l1d_pref; do \
        echo "Indexing $$f"; \
        clang -std=c++20 -fsyntax-only $$f || exit 1; \
    done
	@for f in prefetcher/*.l2c_pref; do \
        echo "Indexing $$f"; \
        clang -std=c++20 -fsyntax-only $$f || exit 1; \
    done
	@for f in prefetcher/*.llc_pref; do \
        echo "Indexing $$f"; \
        clang -std=c++20 -fsyntax-only $$f || exit 1; \
    done


build:
	@echo "Building ChampSim binary..."
	@./build_champsim.sh $(ARCH)_$(CORES)c $(L1D_Pf) $(L2C_Pf) $(LLC_Pf) $(CORES) 2>&1
	@echo "Binary built: $(BIN)"


run:
	@echo "Running $(BIN) with $(CORES) cores..."
	@$(BIN) -warmup_instructions $(WARMUP_instr) \
	          -simulation_instructions $(SIM_instr) \
	          -traces $(TRACE_LIST)
perf_profile:
	@echo "Launching $(BIN) for perf profiling (2 cores, 4M warmup, 9M sim)..."
	@$(BIN) -warmup_instructions 4000000 -simulation_instructions 9000000 \
	    -traces ../traces/LLM256.Pythia-70M_21M.champsimtrace.xz ../traces/LLM256.Pythia-70M_21M.champsimtrace.xz \
	    & SIM_PID=$$!; \
	echo "sim PID=$$SIM_PID, attaching perf for 30s..."; \
	sleep 1; \
	perf record -e cycles:pp --call-graph=fp -F 9999 --mmap-pages=16384 \
	    --pid=$$SIM_PID -o perf.data -- sleep 30; \
	wait $$SIM_PID 2>/dev/null || true; \
	echo "--- perf report (top 40 symbols) ---"; \
	perf report --stdio --no-children -n --sort=symbol -i perf.data 2>/dev/null | head -80

build_run:
	@echo "Building ChampSim binary..."
	@./build_champsim.sh $(ARCH)_$(CORES)c $(L1D_Pf) $(L2C_Pf) $(LLC_Pf) $(CORES) 2>&1
	@echo "Binary built: $(BIN)"
	@echo "Running $(BIN) with $(CORES) cores..."
	@$(BIN) -warmup_instructions $(WARMUP_instr) \
	          -simulation_instructions $(SIM_instr) \
	          -traces $(TRACE_LIST)

print_env:
	@echo BIN=$(BIN)
	@echo ARGS=-warmup_instructions $(WARMUP_instr) -simulation_instructions $(SIM_instr) -traces $(TRACE_LIST)

build_debug:
	@echo "Building ChampSim binary..."
	@./build_champsim.sh $(ARCH)_$(CORES)c $(L1D_Pf) $(L2C_Pf) $(LLC_Pf) $(CORES) 2>&1
	@echo "Binary built: $(BIN)"
	@echo "Launching debugger for $(BIN)..."
	@# CLion can attach debugger to this target
	@$(BIN) -warmup_instructions $(WARMUP_instr) \
	          -simulation_instructions $(SIM_instr) \
	          -traces $(TRACE_LIST)
debug:
	@echo "Launching debugger for $(BIN)..."
	@# CLion can attach debugger to this target
	@$(BIN) -warmup_instructions $(WARMUP_instr) \
	          -simulation_instructions $(SIM_instr) \
	          -traces $(TRACE_LIST)


run_gcc:
	@echo "=== Forcing GCC compilation ==="
	@$(MAKE) FORCE_COMPILER=gcc all

run_aocc:
	@echo "=== Forcing AOCC compilation ==="
	@$(MAKE) FORCE_COMPILER=aocc all

run_clang:
	@echo "=== Forcing Clang compilation ==="
	@$(MAKE) FORCE_COMPILER=clang all

rpass:
	@echo "=== Building with clang -Rpass (optimization remarks) ==="
	@$(MAKE) FORCE_COMPILER=clang CORES=$(CORES) \
	    EXTRA_CFLAGS="-Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize" \
	    build 2>&1 | grep -E "remark:|ooo_cpu|cache\.cc|block\.cc" | head -80

AOCC_PATH := /opt/aocc-compiler-5.0.0/bin/clang++

ifdef FORCE_COMPILER
    ifeq ($(FORCE_COMPILER),aocc)
        ifneq ($(wildcard $(AOCC_PATH)),)
            CXX := $(AOCC_PATH)
            DETECTED := aocc
        else
            $(error AOCC forced but $(AOCC_PATH) not found)
        endif
    else ifeq ($(FORCE_COMPILER),gcc)
        CXX := g++
        DETECTED := gcc
    else ifeq ($(FORCE_COMPILER),clang)
        CXX := clang++
        DETECTED := clang
    else
        $(error Invalid FORCE_COMPILER=$(FORCE_COMPILER). Use 'gcc' or 'aocc')
    endif
else
    ifneq ($(wildcard $(AOCC_PATH)),)
        CXX := $(AOCC_PATH)
        DETECTED := aocc
    else ifneq ($(shell command -v clang++ 2>/dev/null),)
        CXX := clang++
        DETECTED := clang
    else
        CXX := g++
        DETECTED := gcc
    endif
endif

$(info >>> Detected compiler: $(DETECTED) [$(CXX)])


ifeq ($(DETECTED),aocc)
    CFlags += -mllvm -inline-threshold=192 -mllvm -unroll-count=192
    CFlags += -stdlib=libstdc++
    CFlags += -msse -msse2 -msse3 -msse4.1 -msse4.2 -msse4a
    CFlags += -fmax-type-align=64 -faligned-allocation
    CFlags += -falign-functions=32 -falign-loops=32
    CFlags += -fjump-tables
    CFlags += -mllvm -phi-elim-preserve-cmpjmp-glue -mllvm -enable-iv-split -fstruct-layout=9
    CFlags += -mfma -mbmi2 -mlzcnt
    CFlags += -mllvm -merge-constant
    CFlags += -mllvm -enable-gvn-hoist -mllvm -reduce-array-computations=3
    CFlags += -mllvm -do-block-reorder=advanced -mllvm -optimize-strided-mem-cost
    CFlags += -fvectorize
    CFlags += -march=znver3 -mtune=znver3 -mavx2
#     CFlags += -march=native
#     PGO_FILE := ./profiles_v2/999.mem_int-SPECLLM.profdata
#     PGO_FILE := ./profiles_v4/PGO_4_SPEC_LLM.profdata
    PGO_FILE := ./profiles_v10/PGO_10_SPEC_LLM.profdata
    ifneq ($(wildcard $(PGO_FILE)),)
        CFlags += -fprofile-use=$(PGO_FILE)
        LDFlags += -fprofile-use=$(PGO_FILE)
    endif
    LDFlags += -L/opt/aocc-compiler-5.0.0/lib -Wl,-rpath,/opt/aocc-compiler-5.0.0/lib -lc++ -lc++abi
else ifeq ($(DETECTED),clang)
# LDFlags += -rdynamic
CFlags += -stdlib=libstdc++             # !!!!!!!!!!!!!!!!!!!!!!!!!! MUST STAY, OTHER BREAKS THE OUTPUT EQUIVALENCE
LDFlags += -stdlib=libstdc++ -lc++abi   # !!!!!!!!!!!!!!!!!!!!!!!!!! MUST STAY, OTHER BREAKS THE OUTPUT EQUIVALENCE
#     CFlags += -fno-pie
CFlags += -mllvm -inline-threshold=192 -mllvm -unroll-count=192
#     CFlags += -stdlib=libstdc++
# CFlags += -stdlib=libc++
# LDFlags += -stdlib=libc++ -lc++abi
# LDFlags += -no-pie
CFlags  += -fno-semantic-interposition -fvisibility-inlines-hidden -ffunction-sections -fdata-sections
LDFlags += -fuse-ld=lld -Wl,-z,max-page-size=2097152 -Wl,--gc-sections -flto=thin
# LTOTUNE: widen thin-LTO cross-module inlining budget so the hot cross-TU
# call edges (O3_CPU::update_rob -> complete_execution, simulation_loop ->
# CACHE::operate / schedule_memory_instruction lambdas) get imported+inlined.
# IPC-exact (codegen only), no semantic change.
LDFlags += -Wl,-mllvm,-import-instr-limit=600 -Wl,-mllvm,-import-hot-multiplier=60 -Wl,-mllvm,-import-cold-multiplier=2
LDFlags += -Wl,-mllvm,-inline-threshold=600 -Wl,-mllvm,-inlinehint-threshold=900 -Wl,-mllvm,-hot-callsite-threshold=900
CFlags += -march=native -mtune=native -mavx2
#     CFlags += -march=native # -mtune=native # -mavx2
# CFlags += -march=znver3 -mtune=znver3 -mavx2 -fomit-frame-pointer
# CFlags += -msse -msse2 -msse3 -msse4.1 -msse4.2 -msse4a
CFlags += -faligned-allocation
CFlags += -falign-functions=32 -falign-loops=32

CFlags += -mllvm -enable-loopinterchange

CFlags += -mllvm -enable-load-pre
CFlags += -fjump-tables
CFlags += -mllvm -enable-evl-indvar-simplify
CFlags += -mfma -mbmi2 -mlzcnt
CFlags += -mllvm -enable-gvn-hoist

CFlags += -flto=thin
CFlags += -fno-rtti
# CFlags += -ftrivial-auto-var-init=zero
CFlags += -mllvm -enable-interleaved-mem-accesses
CFlags += -mllvm -polly -mllvm -polly-run-dce -mllvm -polly-run-inliner


#     CFlags += -fno-exceptions
#     CFlags += -fvectorize
#     CFlags += -fconstexpr-depth=1024000
#     CFlags += -fconstexpr-steps=1024000

#     # --- HUGE PAGES SUPPORT ---
#     CFlags += -D_GNU_SOURCE
#     LDFlags += -lhugetlbfs
# CFlags += -fno-pie
# LDFlags += -no-pie
# LDFlags += -Wl,--emit-relocs

#     PGO_FILE := ./profiles_v2/999.mem_int-SPECLLM.profdata
#     PGO_FILE := ./profiles_v4/PGO_4_SPEC_LLM.profdata
PGO_FILE := ./profiles_v10/PGO_10_SPEC_LLM.profdata
#     PGO_DIR := ./profiles_v3
#     CFlags += -fprofile-generate=/home/cc/Champsim_V4/champsim_v6_PGO/profiles_v4/
#     LDFlags += -fprofile-generate=/home/cc/Champsim_V4/champsim_v6_PGO/profiles_v4/
   ifneq ($(wildcard $(PGO_FILE)),)
       CFlags += -fprofile-use=$(PGO_FILE)  # -Wall -Wextra -g -fno-omit-frame-pointer # -fsanitize=undefined,address
       LDFlags += -fprofile-use=$(PGO_FILE)  # -fsanitize=undefined,address
   endif


   LDFlags += -lsqlite3
#     LDFlags += -Wl,-zcommon-page-size=2097152 -Wl,-zmax-page-size=2097152
#     CFlags += -g -fsanitize=memory -fsanitize-memory-track-origins=2 # -fsanitize-recover=memory
#     LDFlags  += -fsanitize=memory
#     CFlags += -g -fsanitize=address -fsanitize=undefined -fsanitize=bounds -fsanitize=shift# -fsanitize-memory-track-origins=2 # -fsanitize-recover=memory
#     LDFlags  += -fsanitize=address -fsanitize=undefined -fsanitize=bounds -fsanitize=shift

else ifeq ($(DETECTED),gcc)
    CFlags += -fprofile-use=./profiles_v3/SPEC_LLM_GCC_PROFILE_2
    CFlags += -Wno-error=coverage-mismatch -Wno-error=missing-profile -Wno-missing-profile -Wno-coverage-mismatch
    CFlags += -fbranch-probabilities -fprofile-values
	LDFlags += -lsqlite3
    LDFlags += -fprofile-use=./profiles_v3/SPEC_LLM_GCC_PROFILE_2
    LDFlags += -Wno-error=coverage-mismatch -Wno-error=missing-profile -Wno-missing-profile -Wno-coverage-mismatch
    LDFlags += -fbranch-probabilities -fprofile-values
    CFlags += --param max-inline-insns-auto=192
    CFlags += --param max-unroll-times=192 -Wpadded # -fsanitize=address
    CFlags += -fuse-linker-plugin
#     CFlags += -fprofile-arcs -fprofile-values -fvpt
    
    CFlags += -march=native -mtune=native -ftree-vectorize # -ftree-slp-vectorize
# CFlags += -march=icelake-server -mtune=icelake-server -ftree-vectorize -ftree-loop-vectorize -ftree-slp-vectorize #-fvect-cost-model=unlimited
    # -fopt-info-vec-missed=vec_report.txt
    CFlags += -falign-functions=32 -falign-loops=32 
    CFlags += -fsplit-ivs-in-unroller -fno-common 
    CFlags += -fmerge-constants -foptimize-register-move -frename-registers -fipa-ra  -fgcse-las -fgcse-sm 
    CFlags += -fsched-spec-load -fsched-stalled-insns=24 -fsched-stalled-insns-dep=12  --param sched-pressure-algorithm=1 -fpredictive-commoning 
    CFlags += -fprefetch-loop-arrays --param prefetch-latency=300 # --param simultaneous-prefetches=9 
    CFlags +=  --param ipa-max-aa-steps=2000000 #--param max-aliased-vops=100
    
# CFlags += -msse -msse2 -mssse3 -msse4.1 -msse4.2 -mavx -mavx2 -mfma -mf16c \
-mavx512f -mavx512dq -mavx512cd -mavx512bw -mavx512vl -mavx512ifma \
-mavx512vbmi -mavx512vbmi2 \
-mavx512bitalg -mavx512vpopcntdq \
-maes -mpclmul -msha -mvaes -mvpclmulqdq
    # CFlags += -fcode-hoisting
    # CFlags += -ftree-loop-distribution -ftree-loop-ivcanon
    CFlags += -freorder-blocks-algorithm=stc -Wl,--gc-sections
    CFlags += -fjump-tables
    endif

#     LDFlags += -fprofile-generate=/home/cc/Champsim_V4/champsim_v5_PGO/profiles_v3/SED_FLAG 

#     CFlags += -fprofile-generate=/home/cc/Champsim_V4/champsim_v5_PGO/profiles_v3/SED_FLAG
#     LDFlags += -fprofile-generate=/home/cc/Champsim_V4/champsim_v5_PGO/profiles_v3/SED_FLAG 
    LDFlags += -Wl,-zcommon-page-size=2097152 -Wl,-zmax-page-size=2097152
    LDFlags += -pthread -llzma # -fsanitize=address
    
    #************************ DO NOT EDIT BELOW THIS LINE! ************************
    ifeq ($(debug),1)
    debug=-g
else
    debug=
endif
inc := $(addprefix -I,$(inc))
libs := $(addprefix -l,$(libs))
libDir := $(addprefix -L,$(libDir))
CFlags += -c $(debug) $(inc) $(libDir) $(libs)
sources := $(shell find $(srcDir) -name '*.$(srcExt)')
sources := $(filter-out src/main_helper.cc src/cache_helper.cc src/ooo_cpu_helper.cc src/dram_helper.cc src/main_loop.cc src/main_paging.cc,$(sources))
srcDirs := $(shell find . -name '*.$(srcExt)' -exec dirname {} \; | uniq)
objects := $(patsubst %.$(srcExt),$(objDir)/%.o,$(sources))
ifeq ($(srcExt),cc)
    CC = ccache $(CXX)
    export CCACHE_SLOPPINESS := locale,pch_defines
    export CCACHE_NOHASHDIR := 1
    export CCACHE_COMPILERCHECK := content
else
    CFlags += -std=gnu99
endif

all: $(binDir)/$(app)

$(binDir)/$(app): buildrepo $(objects)
	@mkdir -p `dirname $@`
	@echo "Linking $@..."
	@$(CC) $(objects) $(LDFlags) -o $@

$(objDir)/%.o: %.$(srcExt)
	@echo "Generating dependencies for $<..."
	@$(call make-depend,$<,$@,$(subst .o,.d,$@))
	@echo "Compiling $<..."
	@$(CC) $(CFlags) $< -o $@

clean:
	$(RM) -r $(objDir)

distclean: clean
	$(RM) -r $(binDir)/$(app)

buildrepo:
	@$(call make-repo)

define make-repo
   for dir in $(srcDirs); \
   do \
    mkdir -p $(objDir)/$$dir; \
   done
endef

define make-depend
     $(CC) -MM       \
        -MF $3    \
        -MP       \
        -MT $2    \
        $(CFlags) \
        $1
endef

install_env:
	@echo "Installing build dependencies..."
	@if [ "$$(id -u)" -ne 0 ]; then \
		SUDO="sudo"; \
	else \
		SUDO=""; \
	fi; \
	$$SUDO apt-get update && ( \
	$$SUDO apt-get install -y \
		build-essential \
		g++-12 \
		libstdc++-12-dev \
		liblzma-dev \
		ccache \
		clang \
        gdb \
        gdbserver \
		linux-tools-common \
		linux-tools-generic \
		linux-tools-$$(uname -r) || \
	$$SUDO apt-get install -y \
		build-essential \
		g++ \
		libstdc++-11-dev \
		liblzma-dev \
		ccache \
		clang \
		linux-tools-common \
		linux-tools-generic ) && \
        $$SUDO apt install -y python3-pip && pip3 install pandas --break-system-packages && pip3 install openpyxl --break-system-packages
	@echo "Verifying installations..."
	@which g++ || echo "ERROR: g++ not found"
	@which clang++ || echo "WARNING: clang++ not found"
	@ldconfig -p | grep liblzma || echo "ERROR: liblzma not found"
	@which ccache || echo "WARNING: ccache not found"
	@which perf || echo "WARNING: perf not found (kernel tools may need manual install)"
	@echo "Installation complete. For AOCC, download from: https://www.amd.com/en/developer/aocc.html"