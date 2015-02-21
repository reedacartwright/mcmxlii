
GLIBS=$(shell pkg-config --libs gtkmm-3.0)
GFLAGS=$(shell pkg-config --cflags gtkmm-3.0)
DBUSLIBS=$(shell pkg-config --libs dbus-1)
DBUSFLAGS=$(shell pkg-config --cflags dbus-1)

CXXFLAGS += -std=c++11 -g -O3 -march=native

all: simchcg

simchcg: main.o simchcg.o worker.o rexp.o
	$(CXX) $(CXXFLAGS) -o simchcg main.o simchcg.o worker.o rexp.o $(GLIBS) $(DBUSLIBS)

main.o: main.cc simchcg.h worker.h xorshift64.h
	$(CXX) -c $(CXXFLAGS) $(GFLAGS) $(DBUSFLAGS) main.cc

simchcg.o: simchcg.cc simchcg.h worker.h xorshift64.h
	$(CXX) -c $(CXXFLAGS) $(GFLAGS) $(DBUSFLAGS) simchcg.cc

worker.o: worker.cc worker.h xorshift64.h
	$(CXX) -c $(CXXFLAGS) $(GFLAGS) $(DBUSFLAGS) worker.cc

rexp.o: rexp.cc rexp.h
	$(CXX) -c $(CXXFLAGS) $(GFLAGS) $(DBUSFLAGS) rexp.cc

clean:
	-rm *.o simchcg
