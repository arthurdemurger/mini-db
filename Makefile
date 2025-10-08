CC       := gcc
CFLAGS   := -std=c17 -Wall -Wextra -O2 -g
CPPFLAGS := -Iinclude -Isrc

SRC_DIR  := src
TEST_DIR := tests
FIX_DIR  := $(TEST_DIR)/fixtures

PAGER_SRC := $(SRC_DIR)/pager.c
PAGER_HDR := $(SRC_DIR)/pager.h

FIX_SRC   := $(FIX_DIR)/make_fixtures.c
FIX_BIN   := $(FIX_DIR)/make_fixtures.bin

TEST_SRC  := $(TEST_DIR)/test_pager.c
TEST_BIN  := $(TEST_DIR)/test_pager.bin

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
$(TEST_BIN): $(TEST_SRC) $(PAGER_SRC) $(PAGER_HDR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(TEST_SRC) $(PAGER_SRC)

tests: $(TEST_BIN)

# Build fixtures, build tests, then run tests
run: fixtures tests
	@$(TEST_BIN)

# ---- Cleanup ----
clean:
	@rm -f $(FIX_BIN) $(TEST_BIN)
	@if [ -d "$(FIX_DIR)" ]; then rm -f $(FIX_DIR)/*.db; fi
