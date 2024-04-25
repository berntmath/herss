#/********************************************************************************
# Project:      The Hydraulic Economic River System Simulator (HERSS)
# Filename:     Makefile
# Developer:    Bernt Viggo Matheussen (Bernt.Viggo.Matheussen@aenergi.no)
# Organization: Å Energi, www.ae.no
#
# This software is released under the MIT license:

# Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#********************************************************************************/

CC   = g++
OBJ  =  main.o node.o globalconfig.o line.o reservoir.o dataset.o qmin.o \
		powerstation.o channel.o riversystem.o scenario.o herss.o arraycurve.o
LINKOBJ = ${OBJ}
INCS =  -I.
BIN  = ../herss.exe

# Memory check with Valgrind
# valgrind --leak-check=full --show-leak-kinds=all ../herss.exe global_utahps_hourly.txt

# Optimize for speed using O3 flag.
# CFLAGS = $(INCS) -Wall -O3

# Compile for testing and debugging. 
CFLAGS = $(INCS) -Wall -pg -pedantic 


RM = rm -f
.PHONY: all all-before all-after clean clean-custom
all: all-before ${BIN} all-after
clean:
	${RM} ${OBJ} *.core *.stackdump *~ *.exe
all-clean: clean
	${RM} ${BIN}

$(BIN): $(OBJ)
	$(CC) $(LINKOBJ) -o ${BIN}  $(LIBS) ${CFLAGS}

main.o: main.cpp herss.h
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

herss.o: herss.cpp herss.h
	$(CC) $(CFLAGS) -c herss.cpp -o herss.o

qmin.o: qmin.cpp herss.h
	$(CC) $(CFLAGS) -c qmin.cpp -o qmin.o

arraycurve.o: arraycurve.cpp herss.h arraycurve.h
	$(CC) $(CFLAGS) -c arraycurve.cpp -o arraycurve.o
