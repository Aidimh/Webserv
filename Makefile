NAME = webserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -g

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
	src/ConfFile/init.cpp \
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
	src/Response/helperAutoIndex.cpp \
	src/Response/MethodFactory.cpp \
	src/Response/MultipartUploadStrategy.cpp \
	src/Response/POST.cpp \
	src/Response/Response.cpp \
	src/Signals/signal_handling.cpp \
	src/Logging/Logging.cpp 

OBJS = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OBJS) -o $(NAME)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re

# NAME = webserv

# CXX = c++

# CXXFLAGS = -Werror -Wextra -Wall -std=c++98

# SRCS = main.cpp \
# 	   Error_messages.cpp \
# 	   signal_handling.cpp \
# 	   tools.cpp \
# 	   Conf_file_parsing.cpp \
# 	   validate_conf_file.cpp \
# 	   Engine.cpp \
# 	   location_parsing.cpp \
# 	   CGI_class.cpp \
# 	   src/request/ClientRequest.cpp \
# 	   src/request/RequestHelpers.cpp

# HDRS = header.hpp Error.hpp

# OBJS = $(SRCS:.cpp=.o)

# all : $(NAME)

# $(NAME) : $(HDRS) $(OBJS)
# 	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

# clean : 
# 	rm -rf $(OBJS)

# fclean : clean
# 		rm -rf $(NAME)

# re : fclean all
