CXX := g++
CXXFLAGS := -Wall -Wextra -std=c++98 -Wno-missing-field-initializers
PROGRAM := fledit
SOURCES := fledit.cpp settings.cpp
LIBS := $(shell fltk-config --ldstaticflags)

$(PROGRAM): $(SOURCES)
	$(CXX) $(CXXFLAGS) $^ $(LIBS) -o $@

clean:
	$(RM) $(PROGRAM)
