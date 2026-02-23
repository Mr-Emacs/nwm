CXXFLAGS := -std=c++17 -O2
LDLIBS   := -lX11 -lGL -lGLEW -lGLX $(pkg-config --cflags --libs x11) -g
BINDIR   ?= /usr/local/bin

main: install

install: zoomer
	mkdir -p $(BINDIR)
	install -Dm755 zoomer $(BINDIR)/zoomer
	@echo "Installed zoomer to $(BINDIR)"

zoomer:zoomer.cpp
	$(CXX) $(CXXFLAGS) -o zoomer zoomer.cpp $(LDLIBS)
