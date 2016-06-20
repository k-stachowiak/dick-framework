CXX = clang++

CXXFLAGS = -std=c++14 -Wall -Wextra -g -O0 -DDICK_LOG=4
LDFLAGS = -L. -lm -lallegro_monolith

all: demo

demo: libdick.a demo.o
	$(CXX) $(LDFLAGS) demo.o -o $@ -ldick -lm -lallegro_monolith

demo.o: Makefile demo.cpp dick.h
	$(CXX) $(CXXFLAGS) -o $@ -c demo.cpp

libdick.a: dick.o
	ar cr $@ $^
	ranlib $@

dick.o: Makefile dick.cpp dick.h
	$(CXX) $(CXXFLAGS) -o $@ -c -fPIC dick.cpp

.PHONY: clean distr

libdick.so: dick.o
	$(CXX) -shared -o $@ $^ -Wl,-undefined,dynamic_lookup -lm -lallegro_monolith

clean:
	rm -rf distr
	rm -rf *.o *.so *.a demo

distr: libdick.so libdick.a dick.h demo gui_default.ttf
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

