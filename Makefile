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

CPP				:=	g++

CWD					!=	/bin/pwd
MAGICK_CFLAGS		!=	pkg-config --cflags Magick++
MAGICK_LIBS			!=	pkg-config --libs Magick++
DBUS_CFLAGS			!=  pkg-config --cflags dbus-1
DBUS_LIBS			!=  pkg-config --libs dbus-1
DBUS_TINY_CFLAGS	:=	-I$(PWD)/DBUS-Tiny
DBUS_TINY_LIBS		:=	-L$(PWD)/DBUS-Tiny -Wl,-rpath=$(CWD)/DBUS-Tiny -ldbus-tiny

CPPFLAGS		:= -O3 -fPIC -Wall -Wextra -Werror -Wframe-larger-than=65536 -Wno-error=ignored-qualifiers $(MAGICK_CFLAGS) $(DBUS_TINY_CFLAGS) $(DBUS_CFLAGS) \
					-lssl -lcrypto -lpthread -lboost_system -lboost_program_options -lboost_regex -lboost_thread -lboost_chrono $(MAGICK_LIBS) $(DBUS_TINY_LIBS) $(DBUS_LIBS) \

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
				$(Q) $(CPP) @gcc-warnings $(CPPFLAGS) -c $< -o $@

$(BIN):			$(OBJS) main.o
				$(VECHO) "LD $(OBJS) main.o -> $@"
				$(Q) $(CPP) @gcc-warnings $(CPPFLAGS) $(OBJS) main.o -o $@

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
