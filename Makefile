NAME=tagfs
CFLAGS=`pkg-config --cflags --libs fuse` -I/home/surma/build/tree/include -L/home/surma/build/tree/lib -ldb -ggdb
all:
	gcc *.c -o $(NAME) $(CFLAGS)
run:
	-mkdir ./mount
	-./$(NAME) db ./mount 
	-/bin/bash
	-fusermount -u mount
	-rmdir mount

debug:
	-mkdir ./mount
	-./$(NAME) -d ./db ./mount 
	-rmdir mount

clean:
	@rm -rf $(NAME) mount *~
