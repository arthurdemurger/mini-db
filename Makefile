# ================== Toolchain & flags =========================================
CC      ?= cc
CSTD    ?= -std=c11
BASE_CFLAGS ?= -Wall -Wextra -O2
CFLAGS  ?= $(CSTD) $(BASE_CFLAGS)
LDFLAGS ?=
INCLUDES := -Isrc

# Sanitizers (enable: make ASAN=1 UBSAN=1)
ifeq ($(ASAN),1)
  CFLAGS  += -fsanitize=address -fno-omit-frame-pointer
  LDFLAGS += -fsanitize=address
endif
ifeq ($(UBSAN),1)
  CFLAGS  += -fsanitize=undefined
  LDFLAGS += -fsanitize=undefined
endif

# ================== Output controls ===========================================
# COLOR=0 pour désactiver la couleur ; V=1 pour mode verbeux (commandes affichées)
COLOR ?= 1
V     ?= 0
ifeq ($(COLOR),1)
  C_RESET := \033[0m
  C_DIM   := \033[2m
  C_BOLD  := \033[1m
  C_RED   := \033[31m
  C_GRN   := \033[32m
  C_YLW   := \033[33m
  C_BLU   := \033[34m
  C_CYA   := \033[36m
else
  C_RESET :=
  C_DIM   :=
  C_BOLD  :=
  C_RED   :=
  C_GRN   :=
  C_YLW   :=
  C_BLU   :=
  C_CYA   :=
endif
ifeq ($(V),1)
  Q :=
else
  Q := @
endif

# ================== Sources / objets ==========================================
SRC_CORE := src/pager.c src/table.c src/table_manager.c src/cli_format.c
OBJ_CORE := $(SRC_CORE:.c=.o)


CLI_SRC  := src/main.c
CLI_OBJ  := $(CLI_SRC:.c=.o)

TEST_SRC := tests/test_pager.c tests/test_table.c tests/test_table_manager.c
TEST_BIN := test_pager test_table test_table_manager
TEST_OBJ := $(TEST_SRC:.c=.o)

# Liste complète des objets (pour le compteur i/N)
ALL_OBJS := $(OBJ_CORE) $(CLI_OBJ) $(TEST_OBJ)
TOTAL := $(words $(ALL_OBJS))

# ================== Helpers d’affichage =======================================
# Progression: calcule l’index i de $@ dans ALL_OBJS et affiche [i/N] <fichier>
define show_progress
IDX=$$(echo "$(ALL_OBJS)" | tr ' ' '\n' | nl -w1 -s' ' | awk '$$2=="$@"{print $$1}'); \
printf "$(C_DIM)[%s/$(TOTAL)]$(C_RESET) $(C_CYA)%s$(C_RESET)\n" "$$IDX" "$<";
endef

define show_link
printf "$(C_DIM)[link]$(C_RESET) $(C_BLU)%s$(C_RESET)\n" "$@"
endef

# ================== Phony targets =============================================
.PHONY: all clean test tests scenario help check

# ================== Top-level ==================================================
all: mdb tests
	@printf "$(C_GRN)OK$(C_RESET) built: mdb + tests\n"

mdb: $(CLI_OBJ) $(OBJ_CORE)
	@$(call show_link)
	$(Q)$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@printf "$(C_GRN)OK$(C_RESET) %s\n" "$@"

# ================== Tests ======================================================
tests: $(TEST_BIN)
	@printf "$(C_GRN)OK$(C_RESET) test binaries ready\n"

test_pager: tests/test_pager.o $(OBJ_CORE)
	@$(call show_link)
	$(Q)$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_table: tests/test_table.o $(OBJ_CORE)
	@$(call show_link)
	$(Q)$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_table_manager: tests/test_table_manager.o $(OBJ_CORE)
	@$(call show_link)
	$(Q)$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test: tests
	@printf "$(C_BOLD)Running tests…$(C_RESET)\n"
	$(Q)./test_pager            && printf "$(C_GRN)PASS$(C_RESET) test_pager\n"           || (printf "$(C_RED)FAIL$(C_RESET) test_pager\n"; exit 1)
	$(Q)./test_table            && printf "$(C_GRN)PASS$(C_RESET) test_table\n"           || (printf "$(C_RED)FAIL$(C_RESET) test_table\n"; exit 1)
	$(Q)./test_table_manager    && printf "$(C_GRN)PASS$(C_RESET) test_table_manager\n"   || (printf "$(C_RED)FAIL$(C_RESET) test_table_manager\n"; exit 1)
	@printf "$(C_GRN)All tests passed$(C_RESET)\n"

check: clean
	$(Q)$(MAKE) ASAN=1 UBSAN=1 test scenario

# ================== Scénario ===================================================
scenario: mdb scripts/run_scenario.sh
	@printf "$(C_BOLD)Scenario…$(C_RESET)\n"
	$(Q)./scripts/run_scenario.sh && printf "$(C_GRN)OK$(C_RESET) scenario\n"

# ================== Règles de compilation =====================================
# Règles src/
src/%.o: src/%.c src/pager.h src/table.h src/table_manager.h src/cli_format.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Règles tests/
tests/%.o: tests/%.c src/pager.h src/table.h src/table_manager.h
	@$(call show_progress)
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Objet du CLI
src/main.o: src/main.c src/pager.h src/table_manager.h
	@$(call show_progress)
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# ================== Maintenance ===============================================
clean:
	$(Q)rm -f $(OBJ_CORE) $(CLI_OBJ) $(TEST_OBJ) $(TEST_BIN) mdb
	@printf "$(C_YLW)cleaned$(C_RESET)\n"

help:
	@echo "Targets: all, mdb, tests, test, scenario, check, clean, help"
	@echo "Flags  : COLOR=0 (no color), V=1 (verbose), ASAN=1 UBSAN=1"
