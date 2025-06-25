NAME        := ft_vox
CC          := g++
FLAGS       := -DLINUX -std=c++11 -Wall -Wextra -Werror

# Include paths
INCLUDES    := -I include -I external

# External C sources (e.g. glad.c, stb_image.cpp)
EXT_SRCS := external/glad/glad.cpp external/stb_image/stb_image.cpp
EXT_OBJS := $(EXT_SRCS:.cpp=.o)

# Source and object files
SRCS        := $(wildcard src/*.cpp)
OBJS        := $(SRCS:.cpp=.o)

# All objects
ALL_OBJS    := $(OBJS) $(EXT_OBJS)

# Libraries (link to OpenGL, GLFW, etc.)
LIBS        := -lGL -lglfw -lX11 -lXxf86vm -lXrandr -lpthread -lXi -ldl -lz

# Compile .cpp to .o
%.o: %.cpp
	$(CC) $(FLAGS) $(INCLUDES) -c $< -o $@

# Link all objects into executable
$(NAME): $(ALL_OBJS)
	@echo "\033[1;32mLinking $(NAME)...\033[0m"
	$(CC) $(FLAGS) $(INCLUDES) -o $(NAME) $(ALL_OBJS) $(LIBS)

# Build targets
all: $(NAME)

clean:
	@rm -f $(OBJS) $(EXT_OBJS)
	@echo "\033[1;31mAll object files removed.\033[0m"

fclean: clean
	@rm -f $(NAME)
	@echo "\033[1;31mExecutable removed.\033[0m"

re: fclean all

.PHONY: all clean fclean re
