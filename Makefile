CHAKRA_INCLUDE_DIR ?= /Users/jack/Projects/ChakraCore/lib/Jsrt/ 
CHAKRA_LD_FLAGS ?= -L/Users/jack/Projects/ChakraCore/BuildLinux/Test/bin/ChakraCore/ -lChakraCore 

LIBUV_INCLUDE_DIR = -I /usr/local/Cellar/libuv/1.11.0/include
LIBUV_LD_FLAGS ?= -L/usr/local/Cellar/libuv/1.11.0/lib/ -luv

CURDIR := $(shell pwd)
BASEDIR := $(abspath $(CURDIR)/..)

PROJECT ?= $(notdir $(BASEDIR))
PROJECT := $(strip $(PROJECT))


C_SRC_DIR = $(CURDIR)/src
C_SRC_OUTPUT = bin/couch-chakra
OBJDIR = obj

# System type and C compiler/flags.

UNAME_SYS := $(shell uname -s)
ifeq ($(UNAME_SYS), Darwin)
	CC = cc
	CFLAGS = -O3 -std=c99 -arch x86_64 -finline-functions -Wall -Wmissing-prototypes 
	CXXFLAGS = -O3 -arch x86_64 -finline-functions -Wall
	LDFLAGS = -arch x86_64 -flat_namespace -undefined suppress
else ifeq ($(UNAME_SYS), FreeBSD)
	CC = cc
	CFLAGS = -O3 -std=c99 -finline-functions -Wall -Wmissing-prototypes
	CXXFLAGS = -O3 -finline-functions -Wall
else ifeq ($(UNAME_SYS), Linux)
	CC = gcc
	CFLAGS = -O3 -std=c99 -finline-functions -Wall -Wmissing-prototypes
	CXXFLAGS = -O3 -finline-functions -Wall
endif

CFLAGS += -fPIC -I $(CHAKRA_INCLUDE_DIR) $(LIBUV_INCLUDE_DIR)
CXXFLAGS += -fPIC -I $(CHAKRA_INCLUDE_DIR) $(LIBUV_INCLUDE_DIR)

LDLIBS += $(CHAKRA_LD_FLAGS) $(LIBUV_LD_FLAGS)

# Verbosity.

c_verbose_0 = @echo " C     " $(?F);
c_verbose = $(c_verbose_$(V))

cpp_verbose_0 = @echo " CPP   " $(?F);
cpp_verbose = $(cpp_verbose_$(V))

link_verbose_0 = @echo " LD    " $(@F);
link_verbose = $(link_verbose_$(V))

SOURCES := $(shell find $(C_SRC_DIR) -type f \( -name "*.c" -o -name "*.cpp" \))

OBJECTS = $(addprefix $(OBJDIR)/, $(addsuffix .o, $(basename $(notdir $(SOURCES)))))

COMPILE_C = $(c_verbose) $(CC) $(CFLAGS) $(CPPFLAGS) -c
COMPILE_CPP = $(cpp_verbose) $(CXX) $(CXXFLAGS) $(CPPFLAGS) -c

$(C_SRC_OUTPUT): dist/couch_chakra.js.h $(OBJECTS)
	@mkdir -p bin/
	$(link_verbose) $(CC) $(OBJECTS) $(LDFLAGS) $(LDLIBS) -o $(C_SRC_OUTPUT)

$(OBJDIR)/%.o: $(C_SRC_DIR)/%.c
	@mkdir -p $(OBJDIR) 
	$(COMPILE_CPP) $(OUTPUT_OPTION) $<

$(OBJDIR)/%.o: $(C_SRC_DIR)/%.cpp
	@mkdir -p $(OBJDIR) 
	$(COMPILE_CPP) $(OUTPUT_OPTION) $<

#%(OBJDIR)/%.o: $(C_SRC_DIR)/%.cpp
#	$(COMPILE_CPP) $(OUTPUT_OPTION) $<

#$(OBJDIR)/main.js: js/esprima.js js/escodegen.browser.min.js js/normalizeFunction.js 
#	@mkdir -p $(OBJDIR) 
#	cat $^ > $@
dist/couch_chakra.js:
	npm run compile

dist/couch_chakra.js.h: dist/couch_chakra.js 
	xxd -i $< $@

#$(OBJDIR)/main.js.h: $(OBJDIR)/main.js 
#	xxd -i $< $@

clean:
	@rm -f $(C_SRC_OUTPUT) $(OBJECTS)
	@rm -rf $(OBJDIR) 
	@rm -rf dist

check: $(OBJDIR)/chai.js
	./tests/run.sh

$(OBJDIR)/chai.js:
	curl http://chaijs.com/chai.js > $@
