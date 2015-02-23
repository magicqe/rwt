CXX = g++
SDL_LIB = -L/usr/local/lib -lSDL -lSDL_ttf -lSDL_draw -lcurl -lmpdclient -Wl,-rpath=/usr/local/lib -pthread
SDL_INCLUDE = -I/usr/include
CXXFLAGS = -Wall -c -std=c++0x $(SDL_INCLUDE) -pthread
LDFLAGS = $(SDL_LIB) cJSON.o
EXE = rwt

all: $(EXE)

$(EXE): main.o cJSON.o
	$(CXX) $< $(LDFLAGS) -o $@

main.o: main.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

cJSON.o: cJSON.c cJSON.h
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm *.o && rm $(EXE)
