# Realpath - Emacs dynamic module interface to canonicalize_file_name(3).

# Written in 2017 by Basil L. Contovounesios <basil.conto@gmail.com>
#
# This file is NOT part of Emacs.
#
# To the extent possible under law, the author has dedicated all
# copyright and related and neighbouring rights to this software to
# the public domain worldwide.  This software is distributed without
# any warranty.
#
# You should have received a copy of the CC0 Public Domain Dedication
# along with this software.  If not, see
# <https://creativecommons.org/publicdomain/zero/1.0/>.

# Query Emacs

EMACS  ?= emacs
princ   = $(shell $(EMACS) --batch --eval '(princ $(1))')
either  = $(or $(call princ,$(1)),$(error $(2)))

# Files

MOD_NAME        ?= realpath
mod_suffix_lisp := (or (bound-and-true-p module-file-suffix) "")
mod_suffix_err  := $(EMACS) does not support modules
mod_suffix      := $(call either,$(mod_suffix_lisp),$(mod_suffix_err))
mod_target      := $(addsuffix $(mod_suffix),$(MOD_NAME))

# Dynamic module flags

CFLAGS  += -s -fPIC
LDFLAGS += -shared

# Default warning flags, disabled with WARN=0

ifneq ($(strip $(WARN)),0)
CFLAGS += -Wall -Wextra
endif

# Optional optimisation flags, enabled with OPT=1

ifeq ($(strip $(OPT)),1)
CFLAGS  += -flto -march=native -O2
LDFLAGS += -flto
endif

# Rules

.PHONY: all
all: $(MOD_NAME)

$(mod_target): $(addsuffix .c,$(MOD_NAME))
	$(LINK.c) $< $(LDLIBS) $(OUTPUT_OPTION)

.PHONY: $(MOD_NAME)
$(MOD_NAME): $(mod_target)

.PHONY: clean
clean:
	$(RM) $(mod_target)
