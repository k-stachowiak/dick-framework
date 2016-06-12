CXX = g++

CXXFLAGS = -std=c++14 -Wall -Wextra -g -O0 -DDICK_LOG_ENABLE
LDFLAGS = -lm -lallegro_monolith

all: demo distr

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

libdick.so: dick.o
	$(CXX) -shared -o $@ $^ -lm -lallegro_monolith

libdick.a: dick.o
	ar cr $@ $^
	ranlib $@

dick.o: dick.cpp dick.h
	$(CXX) $(CXXFLAGS) -o $@ -c -fPIC dick.cpp

demo.o: demo.cpp dick.h
	$(CXX) $(CXXFLAGS) -o $@ -c demo.cpp

demo: libdick.a demo.o
	$(CXX) $(CXXFLAGS) demo.o -o $@ -rdynamic libdick.a -lm -lallegro_monolith

.PHONY: clean

clean:
	rm -rf distr
	rm -rf *.o *.so *.a demo

