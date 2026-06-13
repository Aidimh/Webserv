NAME = webserv

CXX = c++

CXXFLAGS = -Werror -Wextra -Wall -std=c++98

SRCS = main.cpp \
	   Error_messages.cpp \
	   signal_handling.cpp \
	   tools.cpp \
	   Conf_file_parsing.cpp \
	   validate_conf_file.cpp \
	   classes_funcs.cpp \

HDRS = header.hpp Error.hpp

OBJS = $(SRCS:.cpp=.o)

all : $(NAME)

$(NAME) : $(HDRS) $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

clean : 
	rm -rf $(OBJS)

fclean : clean
		rm -rf $(NAME)

re : fclean all