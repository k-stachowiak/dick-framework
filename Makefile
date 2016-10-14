CXX = clang++

CXXFLAGS_COMMON = -std=c++14 -Wall -Wextra
CXXFLAGS_RELEASE = $(CXXFLAGS_COMMON) -O2 -DDICK_LOG=1
CXXFLAGS_DEBUG = $(CXXFLAGS_COMMON) -g -O0 -DDICK_LOG=4
LDFLAGS = -L. -lm -lallegro_monolith

all: demo distr

demo: libdickd.a demo.o
	$(CXX) $(LDFLAGS) demo.o -o $@ -ldickd -lm -lallegro_monolith

demo.o: Makefile demo.cpp dick.h
	$(CXX) $(CXXFLAGS_DEBUG) -o $@ -c demo.cpp

libdickd.a: dickd.o
	ar cr $@ $^
	ranlib $@

libdick.a: dick.o
	ar cr $@ $^
	ranlib $@

dick.o: Makefile dick.cpp dick.h
	$(CXX) $(CXXFLAGS_RELEASE) -o $@ -c -fPIC dick.cpp

dickd.o: Makefile dick.cpp dick.h
	$(CXX) $(CXXFLAGS_DEBUG) -o $@ -c -fPIC dick.cpp

libdick.so: dick.o
	$(CXX) -shared -o $@ $^ -Wl,-undefined,dynamic_lookup -lm -lallegro_monolith

libdicks.so: dickd.o
	$(CXX) -shared -o $@ $^ -Wl,-undefined,dynamic_lookup -lm -lallegro_monolith

.PHONY: clean distr

clean:
	rm -rf distr
	rm -rf *.o *.so *.a demo

distr: libdick.so libdick.a libdickd.a dick.h demo gui_default.ttf
	rm -rf $@
	mkdir -p $@/include
	cp dick.h $@/include
	mkdir -p $@/lib
	cp libdick.a $@/lib
	cp libdick.so $@/lib
	mkdir -p $@/share/dick
	cp *.ttf $@/share/dick
	mkdir -p $@/bin
	cp demo gui_default.ttf $@/bin
