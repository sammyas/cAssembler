#
# Based on Makefiles previously written by Julie Zelenski.
#

CXX = g++

# The CXXFLAGS variable sets compile flags for gcc:
#  -g                         compile with debug information
#  -Wall                      give all diagnostic warnings
#  -pedantic                  require compliance with ANSI standard
#  -O0                        do not optimize generated code
#  -std=c++0x                 go with the c++0x extensions for thread support, unordered maps, etc
#  -D_GLIBCXX_USE_NANOSLEEP   included for this_thread::sleep_for and this_thread::sleep_until support
#  -D_GLIBCXX_USE_SCHED_YIELD included for this_thread::yield support
CXXFLAGS = -g -Wall -pedantic -O0 -std=c++0x -D_GLIBCXX_USE_NANOSLEEP -D_GLIBCXX_USE_SCHED_YIELD 

# The LDFLAGS variable sets flags for linker
#  -lm       link in libm (math library)
#  -pthread  link in libpthread (thread library) to back C++11 extensions
#  -lrand    link to simple random number generator
#  -lthreads link to convenience functions layering over C++11 threads
#  -lxml2    link in libxml2 (XML processing library)
LDFLAGS = -lm -pthread -lxml2  -lcurl

# In this section, you list the files that are part of the project.
# If you add/change names of header/source files, here is where you
# edit the Makefile.
SOURCES = \
	cAssembler.cc \
	cadvisor_sources.cc   
HEADERS = \
	cadvisor-sources.h \
	cadvisor-stats.h \
	container.h
	
OBJECTS = $(SOURCES:.cc=.o)
TARGETS = cAssembler

default: $(TARGETS)

cAssembler: cAssembler.o cadvisor_sources.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# In make's default rules, a .o automatically depends on its .c file
# (so editing the .c will cause recompilation into its .o file).
# The line below creates additional dependencies, most notably that it
# will cause the .c to reocmpiled if any included .h file changes.

Makefile.dependencies:: $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -MM $(SOURCES) > Makefile.dependencies

-include Makefile.dependencies

# Phony means not a "real" target, it doesn't build anything
# The phony target "clean" that is used to remove all compiled object files.

.PHONY: clean spartan

clean:
	@rm -f $(TARGETS) $(OBJECTS) core Makefile.dependencies news-aggregator_soln

spartan: clean
	@rm -f *~ .*~

