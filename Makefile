
CC=gcc

CFLAGS=-O2
LIBS=-lX11 -lreadline

OBJN=mem cpu Z80 display io low debugger cpu_print cpu_print_arg alarm cpu_timing

_HEAD=logging registers
HEAD=$(addprefix src/, $(addsuffix .h, $(OBJN) $(_HEAD)))

_OBJS=
OBJS=$(addprefix src/, $(addsuffix .c, $(_OBJS) $(OBJN)))
_OBJC=$(_OBJS)
OBJC=$(addprefix src/objs/, $(addsuffix .o, $(_OBJC) $(OBJN)))



gb : src/main.c $(OBJC) Makefile
	$(CC) src/main.c $(OBJC) -o gb $(CFLAGS) $(LIBS)

src/cpu_timing.c : cpu_timing_generate.py opcode_timings.txt
	rm src/cpu_timing.c;\
	./cpu_timing_generate.py >src/cpu_timing.c

src/objs/%.o : src/%.c $(HEAD) Makefile
	$(CC) $< -c -o $@ $(CFLAGS) $(LIBS)

debug : CFLAGS=-ggdb3 -Wall -Wextra -fsanitize=undefined -fno-sanitize-recover
debug : |gb

clean :
	rm src/objs/* gb

