
CC=gcc

CFLAGS=-O2
LIBS=-lX11 -lreadline

_FILENAMES=mem cpu Z80 display io low debugger cpu_print cpu_print_arg alarm cpu_timing

_HEAD=logging registers
HEAD=$(addprefix src/, $(addsuffix .h, $(_FILENAMES) $(_HEAD)))

_OBJSRC=
OBJSRC=$(addprefix src/, $(addsuffix .c, $(_OBJSRC) $(_FILENAMES)))
_OBJ=$(_OBJSRC)
OBJ=$(addprefix src/objs/, $(addsuffix .o, $(_OBJ) $(_FILENAMES)))



gb : src/main.c $(OBJ) Makefile
	$(CC) src/main.c $(OBJ) -o gb $(CFLAGS) $(LIBS)

src/cpu_timing.c : cpu_timing_generate.py opcode_timings.txt
	rm src/cpu_timing.c;\
	./cpu_timing_generate.py >src/cpu_timing.c

src/objs/%.o : src/%.c $(HEAD) Makefile
	$(CC) $< -c -o $@ $(CFLAGS) $(LIBS)

# debug builds are created with `make debug`
debug : CFLAGS=-ggdb3 -Wall -Wextra -fsanitize=undefined -fno-sanitize-recover
debug : |gb

clean :
	rm src/objs/* gb

