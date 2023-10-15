MAKEFLAGS += --no-builtin-rules

V ?= $(VERBOSE)
ifeq ($(V),1)
	Q :=
	VECHO := @true
	MAKEMINS :=
else
	Q := @
	VECHO := @echo
	MAKEMINS := -s
endif

CCWARNINGS		:=	-Wall -Wextra -Werror \
						-Wformat-overflow=2 -Wshift-overflow=2 -Wimplicit-fallthrough=5 \
						-Wformat-signedness -Wformat-truncation=2 \
						-Wstringop-overflow=4 -Wunused-const-variable=2 -Walloca \
						-Warray-bounds=2 -Wswitch-bool -Wsizeof-array-argument \
						-Wduplicated-branches -Wduplicated-cond -Wlto-type-mismatch -Wnull-dereference \
						-Wdangling-else -Wdangling-pointer=2 \
						-Wpacked -Wfloat-equal -Winit-self -Wmissing-include-dirs \
						-Wmissing-noreturn -Wbool-compare \
						-Wsuggest-attribute=noreturn -Wsuggest-attribute=format -Wmissing-format-attribute \
						-Wuninitialized -Wtrampolines -Wframe-larger-than=2048 \
						-Wunsafe-loop-optimizations -Wshadow -Wpointer-arith -Wbad-function-cast \
						-Wcast-qual -Wwrite-strings -Wsequence-point -Wlogical-op -Wlogical-not-parentheses \
						-Wredundant-decls -Wvla -Wdisabled-optimization \
						-Wunreachable-code -Wparentheses -Wdiscarded-array-qualifiers \
						-Wmissing-prototypes -Wold-style-definition -Wold-style-declaration -Wmissing-declarations \
						-Wcast-align -Winline -Wmultistatement-macros -Warray-bounds=2 \
						\
						-Wno-error=cast-qual \
						-Wno-error=unsafe-loop-optimizations \
						\
						-Wno-packed \
						-Wno-unused-parameter \

CFLAGS			:=	-pipe -Os -g -std=gnu11 -fdiagnostics-color=auto \
						-fno-inline -mlongcalls -mno-serialize-volatile -mno-target-align \
						-fno-math-errno -fno-printf-return-value \
						-ftree-vrp \
						-ffunction-sections -fdata-sections

CPP				:=	g++

MAGICK_CFLAGS	!=	pkg-config --cflags Magick++
MAGICK_LIBS		!=	pkg-config --libs Magick++

CPPFLAGS		:= -O3 -Wall -Wextra -Werror -Wframe-larger-than=65536 -Wno-error=ignored-qualifiers $(MAGICK_CFLAGS) \
					-lssl -lcrypto -lpthread -lboost_system -lboost_program_options -lboost_regex -lboost_thread $(MAGICK_LIBS) \

.PRECIOUS:		*.cpp
.PHONY:			all

all:			espif

clean:
				$(VECHO) "CLEAN"
				-$(Q) rm -f espif.o espif.h.gch espif 2> /dev/null

%:				%.cpp
				$(VECHO) "HOST CPP $<"
				$(Q) $(CPP) $(CPPFLAGS) $< -o $@

%.h.gch:		%.h
				$(VECHO) "HOST CPP PCH $<"
				$(Q) $(CPP) $(CPPFLAGS) -c -x c++-header $< -o $@

espif:			espif.cpp espif.h.gch
