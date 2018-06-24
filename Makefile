FLTK_DIR := fltk-1.3.4-2
FLTK_ARCHIVE := $(FLTK_DIR)-source.tar.gz
FLTK_LIB := $(FLTK_DIR)/lib/libfltk.a

CXX := g++
CXXFLAGS = -isystem $(FLTK_DIR) $(shell $(FLTK_DIR)/fltk-config --cxxflags) -Wall -Wextra -std=c++98 -Wno-missing-field-initializers -g -fsanitize=address
PROGRAM := fledit
SOURCES := fledit.cpp settings.cpp history.cpp colorize.cpp font_dialog.cpp find_dialog.cpp
LIBS = $(shell $(FLTK_DIR)/fltk-config --ldstaticflags)

$(PROGRAM): $(SOURCES) | $(FLTK_LIB)
	$(CXX) $(CXXFLAGS) $(SOURCES) $(LIBS) -o $@

clean:
	$(RM) -r $(PROGRAM) $(FLTK_DIR)

# Build FLTK

$(FLTK_LIB): $(FLTK_DIR)/Makefile
	make -C $(FLTK_DIR)
$(FLTK_DIR)/Makefile: $(FLTK_DIR)
	cd $(FLTK_DIR) && ./configure --disable-shared --enable-debug
	touch $@
$(FLTK_DIR): $(FLTK_ARCHIVE)
	tar -xvf $<
