ifeq ($(SRC_DIR),)
	SRC_DIR := .
endif

DATA_DESTDIR = __DESTDIR
TEST_HTML = my-modified-fc-solve-test.html
RESULT_NODE_JS_EXE = fc-solve.js
RESULT_JS_LIB = libfreecell-solver.js
RESULT_HTML = fc-solve-test.html
PROCESS_PL = $(SRC_DIR)/scripts/process-js-html.pl
EMBED_FILE_MUNGE_PL = $(SRC_DIR)/scripts/emscripten-embed-munge.pl

PATS_C_FILES = pat.c patsolve.c tree.c

LIB_C_FILES = app_str.c scans.c lib.c preset.c instance.c move_funcs_order.c  move_funcs_maps.c meta_alloc.c cmd_line.c card.c state.c check_and_add_state.c fcs_hash.c split_cmd_line.c simpsim.c freecell.c move.c fc_pro_iface.c rate_state.c hacks_for_hlls.c $(PATS_C_FILES)

C_FILES = main.c $(LIB_C_FILES)


SRC_C_FILES = $(patsubst %.c,$(SRC_DIR)/%.c,$(C_FILES))
LLVM_BITCODE_FILES = $(patsubst %.c,%.bc,$(C_FILES))
LLVM_BITCODE_LIB_FILES = $(patsubst %.c,%.bc,$(LIB_C_FILES))

all: $(TEST_HTML) $(RESULT_NODE_JS_EXE) $(RESULT_JS_LIB)

NEEDED_FUNCTIONS = \
	free \
	freecell_solver_user_alloc \
	freecell_solver_user_cmd_line_read_cmd_line_preset \
	freecell_solver_user_cmd_line_parse_args \
	freecell_solver_user_current_state_as_string \
	freecell_solver_user_current_state_as_string \
	freecell_solver_user_free \
	freecell_solver_user_get_next_move \
	freecell_solver_user_get_invalid_state_error_string \
	freecell_solver_user_limit_iterations \
	freecell_solver_user_move_ptr_to_string_w_state \
	freecell_solver_user_resume_solution \
	freecell_solver_user_solve_board \
	malloc \

NEEDED_FUNCTIONS_STR := $(shell perl -e 'print join(", ", map { chr(0x27) . "_" . $$_ . chr(0x27) } @ARGV)' $(NEEDED_FUNCTIONS))

# OPT_FLAGS = -g
OPT_FLAGS = -O2
# OPT_FLAGS = -O3
# OPT_FLAGS = -O1
# OPT_FLAGS =

CFLAGS = $(OPT_FLAGS) -I . -I $(SRC_DIR) -I $(SRC_DIR)/patsolve-shlomif/patsolve/ -m32 -std=gnu99

EMCC_CFLAGS = -s TOTAL_MEMORY="$$((128 * 1024 * 1024))" -s EXPORTED_FUNCTIONS="[$(NEEDED_FUNCTIONS_STR)]" $(CFLAGS)

PRESET_DIR = /fc-solve/share/freecell-solver/
PRESET_FILES_TO_EMBED := $(shell find $(DATA_DESTDIR)$(PRESET_DIR) -type f | (LC_ALL=C sort))

PRESET_FILES_LOCAL := $(shell perl $(EMBED_FILE_MUNGE_PL) $(DATA_DESTDIR) $(PRESET_FILES_TO_EMBED) )

EMCC_POST_FLAGS :=  $(patsubst %,--embed-file %,$(PRESET_FILES_LOCAL))

$(LLVM_BITCODE_FILES): %.bc: $(SRC_DIR)/%.c
	emcc $(EMCC_CFLAGS) $< -c -o $@

.PHONY: llvm_and_files

llvm_and_files: $(LLVM_BITCODE_FILES)

$(RESULT_HTML): llvm_and_files
	emcc $(EMCC_CFLAGS) -o $@  $(LLVM_BITCODE_FILES) $(EMCC_POST_FLAGS)

$(RESULT_NODE_JS_EXE): llvm_and_files
	emcc $(EMCC_CFLAGS) -o $@  $(LLVM_BITCODE_FILES) $(EMCC_POST_FLAGS)

$(RESULT_JS_LIB): llvm_and_files
	emcc $(EMCC_CFLAGS) -o $@  $(LLVM_BITCODE_LIB_FILES) $(EMCC_POST_FLAGS)

$(TEST_HTML): $(RESULT_HTML) $(PROCESS_PL)
	perl $(PROCESS_PL) < $< > $@

SRC_PATS_C_FILES = $(patsubst %.c,$(SRC_DIR)/%.c,$(PATS_C_FILES))

$(SRC_PATS_C_FILES): $(SRC_DIR)/%.c: $(SRC_DIR)/patsolve-shlomif/patsolve/%.c
	cp -f $< $@

clean:
	rm -f $(LLVM_BITCODE_FILES) $(RESULT_HTML) $(RESULT_NODE_JS_EXE)

%.show:
	@echo "$* = $($*)"
