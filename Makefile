CMAKE ?= cmake

BUILDDIR := build/

ANTLRDIR := $(BUILDDIR)/antlr/
ANTLRSRCDIR := $(ANTLRDIR)/src/
ANTLRBUILDDIR := $(ANTLRDIR)/build/

ASMXTOYDIR := $(BUILDDIR)/asmxtoy/
ASMXTOYSRCDIR := $(ASMXTOYDIR)/src/
ASMXTOYBUILDDIR := $(ASMXTOYDIR)/build/

ANTLR_VERSION := 4.13.2


.PHONY: build clean


%/:
	mkdir -p $@

$(ASMXTOYBUILDDIR)/%.cpp.o: $(ASMXTOYSRCDIR)/%.cpp $(ANTLRSRCDIR)/extracted | $(ASMXTOYBUILDDIR)
	$(CXX) -o $@ -c $< -I $(ANTLRSRCDIR)/runtime/src


build: xasm emulator

clean:
	rm -r $(BUILDDIR) 2> /dev/null || true


$(ANTLRDIR)/antlr-$(ANTLR_VERSION)-complete.jar :| $(ANTLRDIR)
	wget https://www.antlr.org/download/antlr-$(ANTLR_VERSION)-complete.jar -O $@


$(ANTLRDIR)/antlr4-cpp-runtime-$(ANTLR_VERSION)-source.zip :| $(ANTLRDIR)
	wget https://www.antlr.org/download/antlr4-cpp-runtime-$(ANTLR_VERSION)-source.zip -O $@

$(ANTLRSRCDIR)/extracted: $(ANTLRDIR)/antlr4-cpp-runtime-$(ANTLR_VERSION)-source.zip
	unzip $< -d $(ANTLRSRCDIR)
	touch $@

$(ANTLRBUILDDIR)/Makefile: $(ANTLRSRCDIR)/extracted | $(ANTLRBUILDDIR)
	$(CMAKE) -S $(ANTLRSRCDIR) -B $(ANTLRBUILDDIR) -DANTLR_BUILD_SHARED=false

$(ANTLRBUILDDIR)/runtime/libantlr4-runtime.a: $(ANTLRBUILDDIR)/Makefile
	cd $</.. && $(MAKE)


$(ASMXTOYSRCDIR)/asmxtoyLexer.cpp $(ASMXTOYSRCDIR)/asmxtoyParser.cpp $(ASMXTOYSRCDIR)/asmxtoyBaseListener.cpp $(ASMXTOYSRCDIR)/asmxtoyLexer.h $(ASMXTOYSRCDIR)/asmxtoyParser.h $(ASMXTOYSRCDIR)/asmxtoyBaseListener.h &: $(ANTLRDIR)/antlr-$(ANTLR_VERSION)-complete.jar asmxtoy.g4  | $(ASMXTOYSRCDIR)
	java -jar $^ -Dlanguage=Cpp -o $(ASMXTOYSRCDIR)

$(ASMXTOYBUILDDIR)/main.cpp.o: main.cpp $(ASMXTOYSRCDIR)/asmxtoyLexer.h $(ASMXTOYSRCDIR)/asmxtoyParser.h $(ASMXTOYSRCDIR)/asmxtoyBaseListener.h $(ANTLRSRCDIR)/extracted | $(ASMXTOYBUILDDIR)
	$(CXX) -o $@ -c $< -I $(ASMXTOYSRCDIR) -I $(ANTLRSRCDIR)/runtime/src 

$(ASMXTOYBUILDDIR)/libasmxtoy.a: $(ASMXTOYBUILDDIR)/asmxtoyLexer.cpp.o $(ASMXTOYBUILDDIR)/asmxtoyParser.cpp.o $(ASMXTOYBUILDDIR)/asmxtoyBaseListener.cpp.o
	$(AR) rcs $@ $^

xasm: $(ASMXTOYBUILDDIR)/main.cpp.o $(ASMXTOYBUILDDIR)/libasmxtoy.a $(ANTLRBUILDDIR)/runtime/libantlr4-runtime.a
	$(CXX) -o $@ $^


$(ASMXTOYBUILDDIR)/emulator.c.o: emulator.c | $(ASMXTOYBUILDDIR)
	$(CC) -o $@ -c $<

emulator: $(ASMXTOYBUILDDIR)/emulator.c.o
	$(CC) -o $@ $^
