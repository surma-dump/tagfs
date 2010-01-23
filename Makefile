NAME=tagfs
all:
	gcc *.c -o $(NAME) `pkg-config --cflags --libs fuse` -ggdb
run:
	-mkdir ./mount
	-./$(NAME) db ./mount 
	-/bin/bash
	-fusermount -u mount
	-rmdir mount

clean:
	@rm -rf $(NAME) mount *~
