# 19 — Editing a header does not rebuild the objects that include it

**Where:** `Makefile`

## Symptom

After adding a member to `Location_Config` in `includes/multiplexing/header.hpp`,
`make` rebuilt only the three `.cpp` files that had been edited, and the server
crashed while parsing a perfectly valid configuration file:

```
Program received signal SIGSEGV, Segmentation fault.
#4  Location_Config::~Location_Config () at includes/multiplexing/header.hpp:87
#9  std::vector<Location_Config>::_M_realloc_insert (...)
#11 parse_config_file () at src/ConfFile/Conf_file_parsing.cpp:355
```

`make re` made it disappear, which is the tell-tale sign.

## Cause

```make
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@
```

The rule depends on the `.cpp` only. Nothing tells `make` that
`src/Routing/Router.o` also depends on `includes/multiplexing/header.hpp`, so a
header change leaves every object that was not otherwise touched stale.

Half the program then believes `sizeof(Location_Config)` is one value and the
other half believes it is another. Objects are constructed with one layout and
destroyed with another — a one-definition-rule violation whose symptom is
memory corruption at an unrelated place, hours after the edit that caused it.

This is not a theoretical risk: it happened during this work, and it will happen
to anyone applying the header changes in
[02](../config/02-location-client-max-body-size.md), [03](../config/03-uninitialised-config-fields.md),
[06](../cgi/06-cgi-connection-dropped.md) or [16](../config/16-return-directive-ignored.md)
without a full rebuild.

## Fix

Let the compiler emit the dependency lists and have `make` read them back.

```make
NAME = webserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -g
DEPFLAGS = -MMD -MP

INCLUDES = \
	-Iincludes \
	-Iincludes/Errors \
	-Iincludes/Methods \
	-Iincludes/Request \
	-Iincludes/Response \
	-Iincludes/Routing \
	-Iincludes/multiplexing \
	-Isrc/Logging

SRCS = \
	main.cpp \
	src/CGI/CGI_class.cpp \
	src/ConfFile/Conf_file_parsing.cpp \
	src/ConfFile/location_parsing.cpp \
	src/ConfFile/tools.cpp \
	src/ConfFile/validate_conf_file.cpp \
	src/Multiplexing/Engine.cpp \
	src/Multiplexing/Error_messages.cpp \
	src/Request/ClientRequest.cpp \
	src/Request/RequestHelpers.cpp \
	src/Routing/Router.cpp \
	src/Response/AMethod.cpp \
	src/Response/BuffersStrategy.cpp \
	src/Response/DeleteMethod.cpp \
	src/Response/Dispatcher.cpp \
	src/Response/GET.cpp \
	src/Response/HeadMethod.cpp \
	src/Response/helperAutoIndex.cpp \
	src/Response/MethodFactory.cpp \
	src/Response/MultipartUploadStrategy.cpp \
	src/Response/POST.cpp \
	src/Response/Response.cpp \
	src/Signals/signal_handling.cpp \
	src/Logging/Logging.cpp

OBJS = $(SRCS:.cpp=.o)
DEPS = $(OBJS:.o=.d)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OBJS) -o $(NAME)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@

-include $(DEPS)

clean:
	rm -f $(OBJS) $(DEPS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
```

`-MMD` writes a `foo.d` next to each `foo.o` listing the project headers it
included; `-MP` adds a phony target for each header so a *deleted* header does
not break the build. `-include $(DEPS)` is silent on the first build, when no
`.d` file exists yet.

`src/Response/HeadMethod.cpp` is added to `SRCS` by
[17](../response/17-head-method-missing.md); `src/Multiplexing/test.cpp` and
`src/Response/BuffersStrategy.cpp` are both empty files —
`BuffersStrategy.cpp` is kept because it is already listed, `test.cpp` is not
listed and can be deleted.

Two more notes on the build:

* `re` is not parallel-safe as written (`make re -j8` links while `fclean` is
  still removing objects, and fails). Adding
  `.NOTPARALLEL: re` avoids the race.
* The commented-out 40-line copy of an older Makefile at the bottom of the file
  is dead weight; version control already remembers it.

## Verification

```
$ touch includes/multiplexing/header.hpp && make
… recompiles all 24 objects …
```

Before the fix the same command recompiled nothing at all.
