# SPDX-License-Identifier: GPL-3.0-or-later
#
# KentOSCad — developer entry point.
#
# This file is a THIN WRAPPER over CMake presets and nothing else. Build logic
# lives in CMakeLists.txt and CMakePresets.json; adding logic here is forbidden
# (.claude/build.md). Every target below is one CMake or ctest invocation.

PRESET  ?= dev
BUILD   := build/$(PRESET)
BIN     := $(BUILD)/bin
JOBS    ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

CMAKE   ?= cmake
CTEST   ?= ctest

.DEFAULT_GOAL := help
.PHONY: help setup build rebuild run run-script test bench check gates \
        format format-check tidy tidy-if-present iwyu-if-present doctor reference \
        clean distclean install package asan headless app docs bench-baseline

## ---------------------------------------------------------------- help ----

help: ## Show this help
	@echo ""
	@echo "  KentOSCad — make targets            (preset: $(PRESET), jobs: $(JOBS))"
	@echo ""
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) \
	  | awk 'BEGIN {FS = ":.*?## "}; {printf "    \033[36m%-16s\033[0m %s\n", $$1, $$2}'
	@echo ""
	@echo "  Presets: dev (default) · debug · release · asan · headless"
	@echo "  Example: make PRESET=release build"
	@echo ""

## --------------------------------------------------------------- build ----

setup: ## Configure the build tree for $(PRESET)
	$(CMAKE) --preset $(PRESET)

$(BUILD)/CMakeCache.txt:
	$(CMAKE) --preset $(PRESET)

build: $(BUILD)/CMakeCache.txt ## Build everything
	$(CMAKE) --build $(BUILD) --parallel $(JOBS)

app: $(BUILD)/CMakeCache.txt ## Build only the application
	$(CMAKE) --build $(BUILD) --target kentos_cad --parallel $(JOBS)

rebuild: distclean build ## Configure from scratch and build

release: ## Build the release preset
	$(MAKE) PRESET=release build

headless: ## Build core + tests without Qt
	$(MAKE) PRESET=headless build

asan: ## Build and test under ASan + UBSan
	$(MAKE) PRESET=asan build
	$(MAKE) PRESET=asan test

## ----------------------------------------------------------------- run ----

run: build ## Launch KentOSCad
	$(BIN)/kentos_cad

run-script: build ## Launch KentOSCad and run SCRIPT=<path> on startup
	@test -n "$(SCRIPT)" || { echo "usage: make run-script SCRIPT=tests/journal/x.json"; exit 2; }
	$(BIN)/kentos_cad --betik "$(SCRIPT)"

## ---------------------------------------------------------------- test ----

test: build ## Run the test suite and the CI gates
	$(CTEST) --test-dir $(BUILD) --output-on-failure

bench: $(BUILD)/CMakeCache.txt ## Run the §10.1 performance budgets
	@$(CMAKE) --build $(BUILD) --target kentos_bench --parallel $(JOBS) >/dev/null
	@$(BIN)/kentos_bench

bench-baseline: $(BUILD)/CMakeCache.txt ## Record this machine's baseline for the regression gate
	@$(CMAKE) --build $(BUILD) --target kentos_bench --parallel $(JOBS) >/dev/null
	@KENTOS_BENCH_RECORD=1 $(BIN)/kentos_bench

gates: ## Run every CI gate script
	@fail=0; for g in scripts/ci-gate-*.sh; do bash "$$g" || fail=1; done; \
	for g in scripts/ci-gate-*.py; do python3 "$$g" || fail=1; done; exit $$fail

check: gates build test format-check tidy-if-present iwyu-if-present ## Everything CI runs, in CI's order

tidy-if-present:
	@if command -v clang-tidy >/dev/null; then \
	    $(MAKE) tidy; \
	else \
	    echo "check: clang-tidy not installed — SKIPPED"; \
	fi

iwyu-if-present:
	@if command -v include-what-you-use >/dev/null; then \
	    iwyu_tool.py -p $(BUILD) src; \
	else \
	    echo "check: include-what-you-use not installed — SKIPPED"; \
	fi

## --------------------------------------------------------------- tools ----

format: ## Reformat all sources with clang-format
	@command -v clang-format >/dev/null || { echo "clang-format not installed"; exit 2; }
	@find src tests \( -name '*.cpp' -o -name '*.hpp' \) -print0 | xargs -0 clang-format -i
	@echo "format: done"

format-check: ## Fail if any source is not formatted
	@command -v clang-format >/dev/null || { echo "format-check: skipped (clang-format missing)"; exit 0; }
	@find src tests \( -name '*.cpp' -o -name '*.hpp' \) -print0 | xargs -0 clang-format --dry-run -Werror

tidy: build ## Run clang-tidy over the compile database
	@command -v clang-tidy >/dev/null || { echo "clang-tidy not installed"; exit 2; }
	@find src -name '*.cpp' | xargs clang-tidy -p $(BUILD) --quiet

doctor: ## Report what this machine can and cannot build
	@scripts/doctor.sh

reference: $(BUILD)/CMakeCache.txt ## Regenerate docs/komutlar/referans.md from the registry
	@$(CMAKE) --build $(BUILD) --target kentos_docgen --parallel $(JOBS) >/dev/null
	@$(BIN)/kentos_docgen docs/komutlar/referans.md

docs: reference ## Regenerate generated docs and check the manual
	@scripts/ci-gate-docs.sh

## --------------------------------------------------------------- dist ----

install: build ## Install into DESTDIR/PREFIX
	$(CMAKE) --install $(BUILD)

package: ## Build a distributable package with CPack
	$(MAKE) PRESET=release build
	cd build/release && cpack

clean: ## Remove build artefacts for $(PRESET)
	@test -d $(BUILD) && $(CMAKE) --build $(BUILD) --target clean || true

distclean: ## Remove the whole build tree
	rm -rf build
