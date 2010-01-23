NAME=tagfs
all:
	gcc *.c -o $(NAME) `pkg-config --cflags --libs fuse` -ggdb
run:
	-./$(NAME) db ./mount 
	-/bin/bash
	-fusermount -u mount
clean:
	@rm -rf $(NAME) mount *~
