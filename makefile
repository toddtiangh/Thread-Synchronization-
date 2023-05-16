override CFLAGS := -Wall -Werror -std=gnu99 -O0 -g $(CFLAGS) -I.

#all: check

#CC = gcc

# Build the threads.o file                                                                                   
threads.o: threads.c threads.h

#busy_threads.o: tests/busy_threads.c threads.h
#full_threads.o: tests/full_threads.c threads.h
#makemore.o: tests/makemore.c threads.h

# make executable                                                                                            
#test_busy_threads: busy_threads.o threads.o
#test_full_threads: full_threads.o threads.o
#test_makemore: makemore.o threads.o

#test_files= ./test_busy_threads ./test_full_threads ./test_makemore

.PHONY: clean check checkprogs all

# Build all of the test programs                                                                             
checkprogs: $(test_files)

# Run the test programs                                                                                      
#check: checkprogs	
#	/bin/bash run_tests.sh $(test_files)

clean:	
	rm -f *.o $(test_files) $(test_o_files)
