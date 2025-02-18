# disable built-in rules
MAKEFLAGS += --no-builtin-rules
.SUFFIXES:
# don't delete intermediate files (.s)
.SECONDARY:

.PHONY: all

all: fish_nda hello

fish_nda : fish_header.o fish_nda.o tools.o
	../elf2omf -o $@ $^

hello: startup.o hello.o
	../elf2omf -S 256 -o $@ $^


#%.o : %.c
#	cc65816 -c --code-model small  --data-model large $^

# for now I 
%.s : %.c
	cc65816 -S --code-model small  --data-model large $^

%.o : %.s
	as65816 -l --code-model small  --data-model large $^