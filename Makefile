NAME=tagfs
BERKDB_INCLUDE=/home/surma/build/tree/include
BERKDB_LIB=/home/surma/build/tree/lib
CFLAGS=-I$(BERKDB_INCLUDE) -L$(BERKDB_LIB) -Wl,-Bstatic -ldb -Wl,-Bdynamic `pkg-config --cflags --libs fuse` -ggdb
all:
	gcc *.c -o $(NAME) $(CFLAGS)
run:
	-mkdir ./mount
	-./$(NAME) -f -o init -b ./db ./mount 
	-rmdir mount

debug:
	-mkdir ./mount
	-./$(NAME) -d ./db ./mount 
	-rmdir mount

clean:
	@rm -rf $(NAME) mount *~
