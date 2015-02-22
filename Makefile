#### RUN PARAMETERS ####

WIDTH=400
HEIGHT=225
MU=1e-5

########################


CXXFLAGS += -std=c++11 -g -O3 -march=native

GLIBS=$(shell pkg-config --libs gtkmm-3.0)
GFLAGS=$(shell pkg-config --cflags gtkmm-3.0)
DBUSLIBS=$(shell pkg-config --libs dbus-1)
DBUSFLAGS=$(shell pkg-config --cflags dbus-1)

all: simchcg

simchcg: main.o simchcg.o worker.o rexp.o
	$(CXX) $(CXXFLAGS) -o simchcg main.o simchcg.o worker.o rexp.o $(GLIBS) $(DBUSLIBS)

main.o: main.cc simchcg.h worker.h xorshift64.h
	$(CXX) -c $(CXXFLAGS) $(GFLAGS) $(DBUSFLAGS) main.cc

simchcg.o: simchcg.cc simchcg.h worker.h xorshift64.h logo.inl
	$(CXX) -c $(CXXFLAGS) $(GFLAGS) $(DBUSFLAGS) simchcg.cc

worker.o: worker.cc worker.h xorshift64.h
	$(CXX) -c $(CXXFLAGS) $(GFLAGS) $(DBUSFLAGS) worker.cc

rexp.o: rexp.cc rexp.h
	$(CXX) -c $(CXXFLAGS) $(GFLAGS) $(DBUSFLAGS) rexp.cc

logo.inl: logo.png
	gdk-pixbuf-csource --raw --name=logo_inline logo.png > logo.inl

logo.png: biodesign_logo_white.pdf
	convert -density 96 biodesign_logo_white.pdf -resize 25% -trim logo.png

clean:
	-rm *.o simchcg logo.inl logo.png

run: simchcg
	./simchcg $(WIDTH) $(HEIGHT) $(MU)
	