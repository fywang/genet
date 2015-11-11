CHARMC  = charmc $(OPTS)
MPICXX  = mpicxx $(OPTS)
OUT    := genet
PART   := gepart

SRC_CI := $(wildcard *.ci)
SRC_C  := $(wildcard *.C)
SRC_CPP:= $(wildcard *.cpp)
INC    := -I./

DECL   := $(SRC_CI:.ci=.decl.h)
DEF    := $(SRC_CI:.ci=.def.h)
OBJ    := $(SRC_C:.C=.o)

CFLAGS     = -O3 -Wall
CHARMFLAGS = -module CkMulticast -language charm++
PROJFLAGS  = -tracemode projections -tracemode summary

LIB        = -std=c++11
LDLIB      = -lm -lyaml-cpp
METISLDLIB = -lparmetis -lmetis

.PHONY: all projections clean

all: $(OUT) $(PART)

$(OUT): $(DECL) $(DEF) $(OBJ)
	$(CHARMC) $(CFLAGS) $(CHARMFLAGS) $(OBJ) $(LDLIB) -o $(OUT)

$(PART): $(SRC_CPP)
	$(MPICXX) $(CFLAGS) $(SRC_CPP) $(INC) $(LIB) $(LDLIB) $(METISLDLIB) -o $(PART)

projections: $(DECL) $(DEF) $(OBJ)
	$(CHARMC) $(CFLAGS) $(PROJFLAGS) $(CHARMFLAGS) $(OBJ) $(LDLIB) -o $(OUT)

%.o: %.C
	$(CHARMC) $(CFLAGS) $(CHARMFLAGS) $(INC) $(LIB) $< -o $@

$(DECL) $(DEF): $(SRC_CI)
	$(CHARMC) $(CFLAGS) $(SRC_CI)

clean:
	rm -f  $(DECL) $(DEF) $(OBJ) $(OUT) $(PART) charmrun
