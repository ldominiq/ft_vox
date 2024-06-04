# c++ -std=c++11 -Werror *.cpp -L Libraries/lib -lglad -lglfw3 -framework OpenGL -framework IOKit -framework Cocoa

NAME        := scop
CC          := c++
FLAGS       := -std=c++11 -Wall -Wextra -Werror

SRCS        := $(wildcard *.cpp)
OBJS        := $(SRCS:.cpp=.o)

.cpp.o:
	$(CC) $(FLAGS) -c $< -o $@

${NAME}:    ${OBJS}
	@echo "\033[1;32mCompilation completed successfully.\033[0m"
	$(CC) $(FLAGS) -o ${NAME} ${OBJS} -L Libraries/lib -lglad -lglfw3

all:        ${NAME}

clean:
	@rm -f *.o
	@echo "\033[1;31mAll object files removed.\033[0m"

fclean:     clean
	@rm -f ${NAME}
	@echo "\033[1;31mExecutable removed.\033[0m"

re:         fclean all

.PHONY:     all clean fclean re
