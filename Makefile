#### RUN PARAMETERS ####

WIDTH=400
HEIGHT=225
MU=4e-6
SCALE=1.44
define DISPLAYMSG
Human and Comparative
Genomics Laboratory
endef
export DISPLAYMSG
SDISPLAYMSG=Human and Comparative\nGenomics Laboratory

########################


CXXFLAGS += -std=c++11 -g -O3 -march=native -Wno-deprecated-declarations
LDFLAGS += -lboost_program_options -lboost_filesystem -lboost_system -lboost_timer

GLIBS=$(shell pkg-config --libs gtkmm-3.0)
GFLAGS=$(shell pkg-config --cflags gtkmm-3.0)
DBUSLIBS=$(shell pkg-config --libs dbus-1)
DBUSFLAGS=$(shell pkg-config --cflags dbus-1)

MAIN=mcmxlii

all: $(MAIN) kiosk.sh

$(MAIN): main.o sim1942.o worker.o rexp.o
	$(CXX) $(CXXFLAGS) -o $(MAIN) main.o sim1942.o worker.o rexp.o $(GLIBS) $(DBUSLIBS) $(LDFLAGS)

main.o: main.cc sim1942.h worker.h xorshift64.h xm.h main.xmh
	$(CXX) -c $(CXXFLAGS) $(GFLAGS) $(DBUSFLAGS) main.cc

sim1942.o: sim1942.cc sim1942.h worker.h xorshift64.h logo.inl
	$(CXX) -c $(CXXFLAGS) $(GFLAGS) $(DBUSFLAGS) sim1942.cc

worker.o: worker.cc sim1942.h worker.h xorshift64.h rexp.h
	$(CXX) -c $(CXXFLAGS) $(GFLAGS) $(DBUSFLAGS) worker.cc

rexp.o: rexp.cc rexp.h
	$(CXX) -c $(CXXFLAGS) $(GFLAGS) $(DBUSFLAGS) rexp.cc

logo.inl: logo.png
	gdk-pixbuf-csource --raw --name=logo_inline logo.png > logo.inl

logo.png: biodesign_logo_white.pdf
	convert -density 96 biodesign_logo_white.pdf -resize 25% -trim logo.png

clean:
	-rm *.o sim1942 kiosk.sh

kiosk.sh: kiosk.sh.in
	sed -e 's/@WIDTH@/$(WIDTH)/' \
	    -e 's/@HEIGHT@/$(HEIGHT)/' \
	    -e 's/@MU@/$(MU)/' \
	    -e 's/@SCALE@/$(SCALE)/' \
	    -e 's|@PREFIX@|$(CURDIR)|' \
	    -e 's/@DISPLAYMSG@/$(SDISPLAYMSG)/' \
	    -e 's/@MAIN@/$(MAIN)/' \
	kiosk.sh.in > kiosk.sh && chmod +x kiosk.sh

############################################################################

run: $(MAIN)
	./$(MAIN) -f -w "$(WIDTH)" -h "$(HEIGHT)" -m "$(MU)" -t "" -s "$(SCALE)"

display: $(MAIN)
	./$(MAIN) -f -w "$(WIDTH)" -h "$(HEIGHT)" -m "$(MU)" -t "$$DISPLAYMSG" -s "$(SCALE)"

video: $(MAIN)
	./$(MAIN) -w 266 -h 200 --win-width=800 --win-height=600 -t "" --delay 10 # this one was used for class
	#./$(MAIN) -w 348 -h 261 --win-width=1392 --win-height=1044 -t ""
	#./$(MAIN) -w 200 -h 150 --win-width=800 --win-height=600 -t "" --delay 5
	#./$(MAIN) -w 266 -h 200 --win-width=800 --win-height=600 -t "" --delay 1

window: $(MAIN)
	./$(MAIN) -w 200 -h 200 -m 1e-5 --win-width=800 --win-height=800 -t "" --delay 1

startx: $(MAIN) kiosk.sh
	startx $(CURDIR)/kiosk.sh -- > kiosk.log

startx2: $(MAIN) kiosk.sh
	startx $(CURDIR)/kiosk.sh high -- > kiosk.log
