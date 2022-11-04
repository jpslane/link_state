all: ls

ls: main.cpp monitor_neighbors.hpp
	g++ -pthread -o ls_router main.cpp monitor_neighbors.hpp

.PHONY: clean
clean:
	rm *.o ls_router
