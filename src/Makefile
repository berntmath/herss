#/********************************************************************************
# Project:      The Hydraulic Economic River System Simulator (HERSS)
# Filename:     Makefile
# Developer:    Bernt Viggo Matheussen, Ove Arstad
# Organization: Å Energi, www.ae.no
#
# This software is released under the MIT license
# Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen, Ove Arstad>

#********************************************************************************/

CC   = g++
#CC   = clang++


OBJ  =  main.o node.o globalconfig.o line.o reservoir.o dataset.o qmin.o \
		powerstation.o channel.o riversystem.o scenario.o herss.o arraycurve.o \
		xtime.o topoparser.o logger.o cascadedreservoirs.o pump.o
INCS =  -I.
BIN  = herss.exe
LIB = herss.so

# TESTS
TEST_DIR = ../src_tests
TEST_SRC = $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJ_FILES = $(patsubst $(TEST_DIR)/%.cpp, %.o, $(TEST_SRC))
TEST_OBJ = $(filter-out main.o, $(OBJ)) $(TEST_OBJ_FILES)
GTEST_DIR = /usr/src/gtest


# Optimize for speed using O3 flag.
# CFLAGS = $(INCS) -Wall -O3

# BVM, May 2026. 
# We have started to use logging inside HERSS. 
# This may slow down computations, but it is very useful for debugging and testing.
# If we want to do sampling or other large runs, we can turn off logging completely. 
# See the file logger.h for more details. 

# Build option, with or without logging, 0,1 (default 0, with logging)
# This will have effect an speed, but it is very useful for debugging and testing.
# First run with logging on your system. Then turn off logging during sampling or other large runs.
NO_LOG ?= 0

# Compile for testing and debugging. 
CFLAGS = $(INCS) -Wall -O3 -pedantic -fPIC -march=native
# These should be tested at some point. But needs proper testing to make sure numerical issues dont polute simulation results. 
# -flto — link-time optimization, can give 5–15% by inlining across translation units
# -ffast-math — allows unsafe float reordering, often 10–30% on numeric code (but check results for correctness)

# Memory check with Valgrind
# valgrind --leak-check=full --show-leak-kinds=all ../herss.exe global_utahps_hourly.txt
CFLAGS = $(INCS) -Wall -g -pedantic -fPIC


# Profiling with gprof (dont use -O3 when compiling for profiling, as it can inline functions and make profiling results less accurate)
#CFLAGS = $(INCS) -Wall -O2 -pg -pedantic -fPIC -march=native
# Run normally then analyse with gprof
#./herss.exe global_utahps_daily.txt
#gprof herss.exe gmon.out | less



ifeq ($(NO_LOG),1)
  CFLAGS += -DHERSS_NO_LOG
endif

TEST_CFLAGS = $(CFLAGS) -I$(GTEST_DIR)/include -pthread


RM = rm -f
.PHONY: all all-before all-after clean clean-custom
all: all-before ${BIN} ${LIB} all-after
clean:
	 ${RM} ${OBJ} *.core *.so *.stackdump *~ *.exe $(TEST_OBJ_FILES)
all-clean: clean
	${RM} ${BIN}

# This makes herss.exe
$(BIN): $(OBJ)
	$(CC) $(OBJ) -o ${BIN} ${CFLAGS}

# This makes herss.so
$(LIB): $(OBJ)
	$(CC) -shared -o ${LIB} ${OBJ}

# TEST TARTGET
test: $(TEST_OBJ) gtest_main.o gtest-all.o
	$(CC) $(TEST_CFLAGS) -o test_runner $^ -lpthread
	./test_runner --gtest_color=yes --gtest_brief=1

gtest_main.a: gtest-all.o
	ar -rv gtest_main.a gtest-all.o

gtest-all.o:
	$(CC) -I$(GTEST_DIR)/include -I$(GTEST_DIR) -c $(GTEST_DIR)/src/gtest-all.cc -o gtest-all.o

gtest_main.o:
	$(CC) $(TEST_CFLAGS) -c $(GTEST_DIR)/src/gtest_main.cc -o gtest_main.o

# Rule to build test object files
%.o: $(TEST_DIR)/%.cpp
	$(CC) $(TEST_CFLAGS) -c $< -o $@


main.o: main.cpp herss.h logger.h
	$(CC) $(CFLAGS) -c main.cpp -o main.o

node.o: node.cpp herss.h
	$(CC) $(CFLAGS) -c node.cpp -o node.o

line.o: line.cpp herss.h
	$(CC) $(CFLAGS) -c line.cpp -o line.o

reservoir.o: reservoir.cpp herss.h
	$(CC) $(CFLAGS) -c reservoir.cpp -o reservoir.o

globalconfig.o: globalconfig.cpp herss.h
	$(CC) $(CFLAGS) -c globalconfig.cpp -o globalconfig.o

dataset.o: dataset.cpp herss.h
	$(CC) $(CFLAGS) -c dataset.cpp -o dataset.o

powerstation.o: powerstation.cpp herss.h
	$(CC) $(CFLAGS) -c powerstation.cpp -o powerstation.o

channel.o: channel.cpp herss.h
	$(CC) $(CFLAGS) -c channel.cpp -o channel.o

riversystem.o: riversystem.cpp herss.h
	$(CC) $(CFLAGS) -c riversystem.cpp -o riversystem.o

scenario.o: scenario.cpp herss.h
	$(CC) $(CFLAGS) -c scenario.cpp -o scenario.o

herss.o: herss.cpp herss.h logger.h
	$(CC) $(CFLAGS) -c herss.cpp -o herss.o

qmin.o: qmin.cpp herss.h
	$(CC) $(CFLAGS) -c qmin.cpp -o qmin.o

xtime.o: xtime.cpp herss.h
	$(CC) $(CFLAGS) -c xtime.cpp -o xtime.o

topoparser.o: topoparser.cpp herss.h
	$(CC) $(CFLAGS) -c topoparser.cpp -o topoparser.o

arraycurve.o: arraycurve.cpp herss.h arraycurve.h
	$(CC) $(CFLAGS) -c arraycurve.cpp -o arraycurve.o

logger.o: logger.cpp herss.h
	$(CC) $(CFLAGS) -c logger.cpp -o logger.o

cascadedreservoirs.o: cascadedreservoirs.cpp herss.h logger.h
	$(CC) $(CFLAGS) -c cascadedreservoirs.cpp -o cascadedreservoirs.o

pump.o: pump.cpp herss.h logger.h
	$(CC) $(CFLAGS) -c pump.cpp -o pump.o

