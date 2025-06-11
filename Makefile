NAME        := ft_vox
CC          := g++
FLAGS       := -DLINUX -std=c++11 -Wall -Wextra -Werror

# Include and library paths
INCLUDES    := -I Libraries -I Libraries/include
LDFLAGS     := -L Libraries/lib

# Static library (.a) included directly
LIBS        := $(LDFLAGS) Libraries/lib/libassimp.a \
               -lGL -lGLU -lglfw3 -lX11 -lXxf86vm -lXrandr -lpthread -lXi -lz

# Source and object files
SRCS        := $(wildcard Sources/*.cpp)
OBJS        := $(SRCS:.cpp=.o)

# Compile .cpp to .o
.cpp.o:
	$(CC) $(FLAGS) $(INCLUDES) -c $< -o $@

# Link objects to final executable
$(NAME): $(OBJS)
	@echo "\033[1;32mCompilation completed successfully.\033[0m"
	$(CC) $(FLAGS) $(INCLUDES) -o $(NAME) $(OBJS) $(LIBS)

# Build targets
all: $(NAME)

clean:
	@rm -f $(OBJS)
	@echo "\033[1;31mAll object files removed.\033[0m"

fclean: clean
	@rm -f $(NAME)
	@echo "\033[1;31mExecutable removed.\033[0m"

re: fclean all

.PHONY: all clean fclean re
