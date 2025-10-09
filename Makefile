CC       := gcc
CFLAGS   := -std=c17 -Wall -Wextra -O2 -g
CPPFLAGS := -Iinclude -Isrc

SRC_DIR  := src
TEST_DIR := tests
FIX_DIR  := $(TEST_DIR)/fixtures

PAGER_SRC := $(SRC_DIR)/pager.c
PAGER_HDR := $(SRC_DIR)/pager.h

TABLE_SRC := $(SRC_DIR)/table.c
TABLE_HDR := $(SRC_DIR)/table.h $(SRC_DIR)/endian_util.h

FIX_SRC   := $(FIX_DIR)/make_fixtures.c
FIX_BIN   := $(FIX_DIR)/make_fixtures.bin

TEST_PAGER_SRC  := $(TEST_DIR)/test_pager.c
TEST_PAGER_BIN  := $(TEST_DIR)/test_pager.bin

TEST_TABLE_SRC  := $(TEST_DIR)/test_table.c
TEST_TABLE_BIN  := $(TEST_DIR)/test_table.bin

.PHONY: all fixtures tests run clean

# Build fixtures and tests by default
all: fixtures tests

# ---- Fixtures ----
$(FIX_BIN): $(FIX_SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $<

fixtures: $(FIX_BIN)
	@mkdir -p $(FIX_DIR)
	@./$(FIX_BIN)

# ---- Tests ----
$(TEST_PAGER_BIN): $(TEST_PAGER_SRC) $(PAGER_SRC) $(PAGER_HDR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(TEST_PAGER_SRC) $(PAGER_SRC)

$(TEST_TABLE_BIN): $(TEST_TABLE_SRC) $(TABLE_SRC) $(TABLE_HDR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(TEST_TABLE_SRC) $(TABLE_SRC)

tests: $(TEST_PAGER_BIN) $(TEST_TABLE_BIN)

# Build fixtures, build tests, then run tests
run: fixtures tests
	@$(TEST_PAGER_BIN)
	@$(TEST_TABLE_BIN)

# ---- Cleanup ----
clean:
	@rm -f $(FIX_BIN) $(TEST_PAGER_BIN) $(TEST_TABLE_BIN)
	@if [ -d "$(FIX_DIR)" ]; then rm -f $(FIX_DIR)/*.db; fi
