CXX = g++
CXXFLAGS = -std=c++11 -O3 -static

all:
	@echo "Bitte wählen Sie das Ziel aus:"
	@echo "  make windows  - Kompiliert statisch für Windows (erzeugt force.exe)"
	@echo "  make linux    - Kompiliert statisch für Linux (erzeugt force)"

windows:
	$(CXX) force.cpp -o force.exe $(CXXFLAGS) -lws2_32

linux:
	$(CXX) force.cpp -o force $(CXXFLAGS) -lpthread

clean:
	rm -f force force.exe