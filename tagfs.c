#define FUSE_USE_VERSION 26
#include <stdio.h>
#include <fuse.h>
#include <db.h>

#define FUSE_OP(a) .a = &tagfs_##a 

static inline char *tagfs_get_db_file() {
	return (char *) fuse_get_context()->private_data ;
}

static void *tagfs_init(struct fuse_conn_info *conn) {
}

static int tagfs_getattr(const char *path, struct stat *stbuf) {
	stat(path, stbuf) ;
	return 0;
}

static int tagfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi) {
	filler(buf, ".", NULL, 0) ;
	filler(buf, "..", NULL, 0) ;

	return 0 ;
}

static struct fuse_operations tagfs_ops = {
	FUSE_OP(init),
	FUSE_OP(getattr),
	FUSE_OP(readdir)
} ;

int main(int argc, char **argv) {
	int i ;
	struct fuse_args args = FUSE_ARGS_INIT(0, NULL) ;

	for(i = 0; i < argc - 2; i++) 
		fuse_opt_add_arg(&args, argv[i]) ;
	fuse_opt_add_arg(&args, argv[argc-1]) ;

	return fuse_main(args.argc, args.argv, &tagfs_ops, argv[argc-2]) ;
}
