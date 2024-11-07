CFLAGS = -g -Wall -std=c++17 -mpopcnt -march=native -pthread

all: sim 
sim: coordinate.h thread_pool.h utils.h sim.cpp 
	g++ $(CFLAGS)  -o sim sim.cpp 

clean:
	rm -f sim