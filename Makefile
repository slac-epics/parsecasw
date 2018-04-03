#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
# National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
# Operator of Los Alamos National Laboratory.
# This file is distributed subject to a Software License Agreement found
# in the file LICENSE that is included with this distribution. 
#*************************************************************************
# Makefile for parsecasw

TOP = .
include $(TOP)/configure/CONFIG

# Optimization
ifdef WIN32
#HOST_OPT = NO
else
#HOST_OPT = NO
endif

# Source browser options
ifeq ($(HOST_OPT),NO)
  ifeq ($(ANSI),ACC)
    ifeq ($(OS_CLASS),solaris)
      USR_CFLAGS += -xsb
    endif
  endif
endif

# Eliminate warnings for deprecated conversion from string constant to
# char *
G++_WARN_YES:=$(subst -Wwrite-strings,,$(G++_WARN_YES))

# Sun compatibility
#USR_CXXFLAGS_solaris+=-compat=4
# Needed for Motif functions using string literals for char * arguments
# Weaker than -compat=4
ifeq ($(ANSI),ACC)
USR_CXXFLAGS_solaris+=-features=no%conststrings
endif

#EPICS_BASE = /home/phoebus/EVANS/epics/baseLatest

# Kludge to avoid compiler warning for -I with nothing after
# (We don't need to include anything from base)
#EPICS_BASE_INCLUDE = .

#STATIC_BUILD = YES 
CMPLR = STRICT
CXXCMPLR = STRICT

# Purify
#PURIFY=YES
ifeq ($(PURIFY),YES)
ifeq ($(OS_CLASS),solaris)
DEBUGCMD = purify -first-only -chain-length=26 $(PURIFY_FLAGS)
#DEBUGCMD = purify -first-only -chain-length=26 -max_threads=256 $(PURIFY_FLAGS)
#DEBUGCMD = purify -first-only -chain-length=26 -always-use-cache-dir -cache-dir=/tmp/purifycache $(PURIFY_FLAGS)
#DEBUGCMD = purify -first-only -chain-length=26 -enable-new-cache-scheme $(PURIFY_FLAGS)
#DEBUGCMD = purify -first-only -chain-length=26 -enable-new-cache-scheme -always-use-cache-dir -cache-dir=/tmp/purifycache $(PURIFY_FLAGS)

# Put the cache files in the appropriate bin directory
PURIFY_FLAGS += -always-use-cache-dir -cache-dir=$(shell $(PERL) $(TOP)/config/fullPathName.pl .)
endif
endif

PROD_HOST := parsecasw

USR_INCLUDES = -I$(MOTIF_INC) -I$(X11_INC)

USR_LIBS_DEFAULT = Com
USR_LIBS_Linux = Com
USR_LIBS_WIN32 = Com

# For SciPlot
USR_CFLAGS += -DMOTIF
USR_CXXFLAGS += -DMOTIF

WIN32_RUNTIME=MD
USR_CFLAGS_WIN32 += /DWIN32 /D_CONSOLE
USR_CXXFLAGS_WIN32 += /DWIN32 /D_CONSOLE
USR_LDFLAGS_WIN32 += /SUBSYSTEM:CONSOLE

Com_DIR = $(EPICS_BASE_LIB)

parsecasw_SRCS += parsecasw.cpp
parsecasw_SRCS += CIoc.cpp
parsecasw_SRCS += utils.cpp

RCS_WIN32 += parsecasw.rc

include $(TOP)/configure/RULES
include $(TOP)/configure/RULES_TOP

parsecasw.res:../parsecasw.ico

