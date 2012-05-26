CXX = g++
CXXFLAGS = -g -O0 -Wall -O3

TARGET_NAME = ct

all = $(TARGET_NAME)

ct : main.o codetorrent.o nc.o galois.o 
	$(CXX) $(CXXFLAGS) -o $@ $^ -lpthread

main.o : main.cpp
	$(CXX) $(CXXFLAGS) -c $<

codetorrent.o : codetorrent.cpp
	$(CXX) $(CXXFLAGS) -c $<

nc.o : nc.cpp
	$(CXX) $(CXXFLAGS) -c $<

galois.o : galois.cpp
	$(CXX) $(CXXFLAGS) -c $<

clean:
	rm -f *.o $(TARGET_NAME)
