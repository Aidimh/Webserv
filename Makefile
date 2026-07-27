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

NAME = WebServTestMakefile

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

SRC_DIR = src
INC_DIR = includes

METHODS = \
	$(SRC_DIR)/Methods/AMethod.cpp \
	$(SRC_DIR)/Methods/GET.cpp \
	$(SRC_DIR)/Methods/POST.cpp \
	$(SRC_DIR)/Methods/DeleteMethod.cpp \
	$(SRC_DIR)/Methods/Dispatcher.cpp \
	$(SRC_DIR)/Methods/MethodFactory.cpp \
	$(SRC_DIR)/Methods/MultipartUploadStrategy.cpp

REQUEST = \
	$(SRC_DIR)/Request/ClientRequest.cpp \
	$(SRC_DIR)/Request/RequestAdapter.cpp \
	$(SRC_DIR)/Request/RequestHelpers.cpp

RESPONSE = \
	$(SRC_DIR)/Response/Response.cpp \
	$(SRC_DIR)/Response/BuffersStrategy.cpp \
	$(SRC_DIR)/Response/helperAutoIndex.cpp

SRCS = \
	main.cpp \
	$(METHODS) \
	$(REQUEST) \
	$(RESPONSE)

OBJS = $(SRCS:.cpp=.o)

INCLUDES = -I$(INC_DIR)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
