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

CPP				:=	g++

MAGICK_CFLAGS	!=	pkg-config --cflags Magick++
MAGICK_LIBS		!=	pkg-config --libs Magick++

CPPFLAGS		:= -O3 -fPIC -Wall -Wextra -Werror -Wframe-larger-than=65536 -Wno-error=ignored-qualifiers $(MAGICK_CFLAGS) \
					-lssl -lcrypto -lpthread -lboost_system -lboost_program_options -lboost_regex -lboost_thread $(MAGICK_LIBS) \

OBJS			:= espif.o espifconfig.o generic_socket.o packet.o util.o exception.o
HDRS			:= espif.h espifconfig.h generic_socket.h packet.h util.h exception.h
BIN				:= espif
SWIG_DIR		:= Esp
SWIG_SRC		:= Esp\:\:IF.i
SWIG_PM			:= IF.pm
SWIG_PM_2		:= $(SWIG_DIR)/IF.pm
SWIG_WRAP_SRC	:= Esp\:\:IF_wrap.cpp
SWIG_WRAP_OBJ	:= Esp\:\:IF_wrap.o
SWIG_SO			:= Esp\:\:IF.so
SWIG_SO_2		:= $(SWIG_DIR)/IF.so

.PRECIOUS:		*.cpp *.i
.PHONY:			all swig

all:			$(BIN) swig

swig:			$(SWIG_PM_2) $(SWIG_SO_2)

clean:
				$(VECHO) "CLEAN"
				-$(Q) rm -rf $(OBJS) main.o $(BIN) $(SWIG_WRAP_SRC) $(SWIG_PM) $(SWIG_PM_2) $(SWIG_WRAP_OBJ) $(SWIG_SO) $(SWIG_SO_2) $(SWIG_DIR) 2> /dev/null

espif.o:		$(HDRS)
espifconfig.o:	$(HDRS)
generic_socket.o: $(HDRS)
main.o:			$(HDRS)
packet.o:		$(HDRS)
util.o:			$(HDRS)
$(SWIG_PM):		$(HDRS)
$(SWIG_SRC):	$(HDRS)

%.o:			%.cpp
				$(VECHO) "CPP $< -> $@"
				$(Q) $(CPP) $(CPPFLAGS) -c $< -o $@

$(BIN):			$(OBJS) main.o
				$(VECHO) "LD $(OBJS) main.o -> $@"
				$(Q) $(CPP) $(CPPFLAGS) $(OBJS) main.o -o $@

$(SWIG_WRAP_SRC) $(SWIG_PM): $(SWIG_SRC)
				$(VECHO) "SWIG $< -> $@"
				$(Q) swig -c++ -cppext cpp -perl5 $<

$(SWIG_WRAP_OBJ):	$(SWIG_WRAP_SRC)
				$(VECHO) "SWIG CPP $< -> $@"
				$(Q) $(CPP) $(CPPFLAGS) -Wno-unused-parameter \
						`perl -MConfig -e 'print join(" ", @Config{qw(ccflags optimize cccdlflags)}, "-I$$Config{archlib}/CORE")'` -c $< -o $@

$(SWIG_SO):		$(SWIG_WRAP_OBJ) $(OBJS)
				$(VECHO) "SWIG LD $< -> $@"
				$(Q) $(CPP) $(CPPFLAGS) `perl -MConfig -e 'print $$Config{lddlflags}'` $(SWIG_WRAP_OBJ) $(OBJS) -o $@

$(SWIG_PM_2):	$(SWIG_PM)
				$(VECHO) "SWIG FINISH PM $< -> $@"
				mkdir -p Esp
				cp $(SWIG_PM) $(SWIG_PM_2)

$(SWIG_SO_2):	$(SWIG_SO) $(SWIG_PM)
				$(VECHO) "SWIG FINISH SO $< -> $@"
				mkdir -p Esp
				cp $(SWIG_SO) $(SWIG_SO_2)
