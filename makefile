CXX = /usr/local/Cellar/llvm/17.0.6_1/bin/clang++
CXXFLAGS = -std=c++20
# CC = $(CXX)

.PHONY: all
all: elf2omf

.PHONY: clean
clean:
	$(RM) elf2omf elf2omf.o omf.o

elf2omf : elf2omf.o omf.o
	$(LINK.cpp) -o $@ $^ 
omf.o: omf.cpp
elf2omf.o : elf2omf.cpp
