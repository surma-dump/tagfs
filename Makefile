NAME=tagfs
CFLAGS=`pkg-config --cflags --libs fuse` -I/home/surma/build/tree/include -L/home/surma/build/tree/lib -ldb
all:
	gcc *.c -o $(NAME) $(CFLAGS)
run:
	-mkdir ./mount
	-LD_LIBRARY_PATH=/home/surma/build/tree/lib ./$(NAME) db ./mount 
	-/bin/bash
	-fusermount -u mount
	-rmdir mount

debug:
	-mkdir ./mount
	-LD_LIBRARY_PATH=/home/surma/build/tree/lib ./$(NAME) -d db ./mount 
	-rmdir mount

clean:
	@rm -rf $(NAME) mount *~
