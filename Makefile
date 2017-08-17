NAME	= universal_server
SRC	= server.c

all:
	gcc -g -ggdb3 -W -Wall $(SRC) -o $(NAME)
