CHARMC   = charmc $(OPTS)
MPICXX   = mpicxx $(OPTS)
OUT     := genet
MOD     := libmodule$(OUT).a

SRC_CI  := $(wildcard *.ci)
SRC_C   := $(wildcard *.C)
SRC_CPP := $(wildcard *.cpp)
INC     := -I./ -I$(CHARMDIR)/include

DECL    := $(SRC_CI:.ci=.decl.h)
DEF     := $(SRC_CI:.ci=.def.h)
OBJ     := $(SRC_C:.C=.o)
OBJ_CPP := $(SRC_CPP:.cpp=.o)

CFLAGS     = -O3 -Wall
CHARMFLAGS = -module CkMulticast -language charm++
MODFLAGS   = -mpi -nomain-module -module genet
PROJFLAGS  = -tracemode projections -tracemode summary

LIB        = -std=c++11
LDLIB      = -lm -lyaml-cpp -lparmetis -lmetis

.PHONY: all projections clean

all: $(OUT)

$(OUT): $(DECL) $(DEF) $(OBJ) $(OBJ_CPP) $(MOD)
	$(CHARMC) $(CFLAGS) $(MODFLAGS) $(CHARMFLAGS) $(OBJ) $(OBJ_CPP) $(LDLIB) -o $(OUT)

$(PART): $(SRC_CPP)

projections: $(DECL) $(DEF) $(OBJ)
	$(CHARMC) $(CFLAGS) $(PROJFLAGS) $(CHARMFLAGS) $(OBJ) $(LDLIB) -o $(OUT)

%.o: %.C
	$(CHARMC) $(CFLAGS) $(CHARMFLAGS) $(INC) $(LIB) $< -o $@

%.o: %.cpp
	$(MPICXX) $(CFLAGS) $(INC) $(LIB) -c $< -o $@

$(MOD): $(OBJ)
	$(CHARMC) $(CFLAGS) $(CHARMFLAGS) $(OBJ) -o $(MOD)

$(DECL) $(DEF): $(SRC_CI)
	$(CHARMC) $(CFLAGS) $(SRC_CI)

clean:
	rm -f  $(DECL) $(DEF) $(OBJ) $(OBJ_CPP) $(MOD) $(OUT) charmrun

# OS related
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  LIB += -stdlib=libc++ -mmacosx-version-min=13.0
endif
