# -*- mode: makefile-gmake -*-
ifndef LEGATE_CORE_DIR
export LEGATE_CORE_DIR := $(shell ./scripts/get_legate_core_dir.py)
endif

ifndef LEGATE_CORE_ARCH
$(error LEGATE_CORE_ARCH not defined)
endif

.SUFFIXES:
.DELETE_ON_ERROR:
.NOTPARALLEL: # parallelism is handled otherwise

.DEFAULT_GOAL := all

JOBS =
ifeq (4.2,$(firstword $(sort $(MAKE_VERSION) 4.2)))
  # Since make 4.2:
  #
  # * The amount of parallelism can be determined by querying MAKEFLAGS, even when
  # the job server is enabled (previously MAKEFLAGS would always contain only
  # "-j", with no number, when job server was enabled).
  #
  # https://lists.gnu.org/archive/html/info-gnu/2016-05/msg00013.html
  JOBS = $(patsubst -j%,%,$(filter -j%,$(MAKEFLAGS)))
endif

ifeq ($(strip $(JOBS)),)
  # Parse the number of jobs from inspecting process list
  MAKE_PID = $(shell echo $$PPID)
  ifeq ($(strip $(MAKE_PID)),)
    JOBS =
  else
    SED ?= sed
    PS ?= ps
    JOBS = $(strip $(shell $(PS) T | $(SED) -n 's%.*$(MAKE_PID).*$(MAKE).* \(-j\|--jobs\) *\([0-9][0-9]*\).*%\2%p'))
  endif
endif
export CMAKE_BUILD_PARALLEL_LEVEL ?= $(JOBS)

include $(LEGATE_CORE_DIR)/$(LEGATE_CORE_ARCH)/gmakevariables

ifeq ($(strip $(V)),1)
export VERBOSE ?= 1
else ifeq ($(strip $(V)),0)
export VERBOSE ?= 0
endif

## Option types:
##
## x|y|z|...      - Mutually exclusive literal values. The option expects either x or
##                  y or z etc.
## /absolute/path - An absolute file-system path. The path must exist (the ultimate
##                  consumer of the option may or may not check for existence), and must
##                  be fully resolved (the ultimate consumer of the option may or may not
##                  do any expansions).
## /any/path      - Any kind of path. The path must exist (the ultimate consumer of the
##                  option may or may not check for existence), but it may be either
##                  relative or absolute. If relative, the path is resolved relative to
##                  the location of this Makefile.
## directory      - The literal name of a directory. This must not be a path. The
##                  directory may or may not exist, see specific option help text for
##                  further guidance.
## '...'          - Any arbitrary string of arguments. For example '-foo -bar -baz'.
##
## Common options for all commands:
##
## - VERBOSE=0|1                    - Whether to enable verbose output.
## - V=0|1                          - Alias for VERBOSE.
## - LEGATE_CORE_DIR=/absolute/path - Override (or set) the root directory for Legate.Core.
## - LEGATE_CORE_ARCH=directory     - Override (or set) the current arch directory. The
##                                    arch directory must exist.

## Print this help message.
##
.PHONY: help
help: default_help

## Build the library.
##
## Options:
## - LEGATE_CORE_CMAKE_ARGS='...' - Any additional arguments to pass to the cmake command.
##
.PHONY: all
all: default_all

## Remove build artifacts.
##
## Options:
## - LEGATE_CORE_CMAKE_ARGS='...' - Any additional arguments to pass to the cmake command.
##
.PHONY: clean
clean: default_clean
	@$(CMAKE) -E rm -rf -- $(LEGATE_CORE_DIR)/legate_core.egg-info
	@$(CMAKE) -E rm -rf -- $(LEGATE_CORE_DIR)/$(LEGATE_CORE_ARCH)/_skbuild


.PHONY: install_private
install_private: all
	@$(MAKE) --no-print-directory default_install

## Install the library.
##
## Options:
## - LEGATE_CORE_CMAKE_ARGS='...'   - Any additional arguments to pass to the cmake command.
## - PREFIX=/any/path               - Set installation prefix of the final install.
## - DESTDIR=/any/path              - Alias for PREFIX.
## - CMAKE_INSTALL_PREFIX=/any/path - If cmake version >= 29, alias for PREFIX,
##                                    otherwise has no effect.
##
.PHONY: install
install: install_private

.PHONY: package_private
package_private: all
	@$(MAKE) --no-print-directory default_package

## Create an installable package of the library.
##
## Options:
## - LEGATE_CORE_CMAKE_ARGS='...' - Any additional arguments to pass to the cmake command.
##
.PHONY: package
package: package_private

## Run clang-tidy over the repository.
##
## Options:
## - LEGATE_CORE_CMAKE_ARGS='...' - Any additional arguments to pass to the cmake command.
##
.PHONY: tidy
tidy:
	@$(LEGATE_CORE_BUILD_COMMAND) --target tidy $(LEGATE_CORE_CMAKE_ARGS)

## Run clang-tidy only over the files which have been changed by your branch.
##
## Beware that this target may not fully work. Turns out asking git "which base branch is
## my current branch based off of" is more or less impossible to answer reliably... so
## depending on which branch it selects, this might not do what you want. It may try to
## check more files than needed (worst case, all of them), or it may not check all
## files. Buyer beware.
##
## Options:
## - LEGATE_CORE_CMAKE_ARGS='...' - Any additional arguments to pass to the cmake command.
##
.PHONY: tidy-diff
tidy-diff:
	@$(LEGATE_CORE_BUILD_COMMAND) --target tidy-diff $(LEGATE_CORE_CMAKE_ARGS)

## Generate raw doxygen output.
##
## Options:
## - LEGATE_CORE_CMAKE_ARGS='...' - Any additional arguments to pass to the cmake command.
##
.PHONY: doxygen
doxygen:
	@$(LEGATE_CORE_BUILD_COMMAND) --target Doxygen $(LEGATE_CORE_CMAKE_ARGS)

## Build combined Sphinx documentation.
##
## Options:
## - LEGATE_CORE_CMAKE_ARGS='...' - Any additional arguments to pass to the cmake command.
##
.PHONY: docs
docs: doxygen
	@$(LEGATE_CORE_BUILD_COMMAND) --target Sphinx $(LEGATE_CORE_CMAKE_ARGS)

## Serve combined Sphinx documentation.
##
## Options:
## - LEGATE_CORE_CMAKE_ARGS='...' - Any additional arguments to pass to the cmake command.
##
.PHONY: docserve
docserve:
	@$(PYTHON) -m http.server -d $(LEGATE_CORE_DIR)/$(LEGATE_CORE_ARCH)/cmake_build/docs/legate/core/sphinx
