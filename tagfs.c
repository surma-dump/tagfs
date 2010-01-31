#define FUSE_USE_VERSION 26
#include <stdlib.h> // getuid() getgid()
#include <sys/types.h>

#include <fuse.h> // FUSE... duh
#include <db.h> // BerkeleyDB
#include <errno.h> // Return values

#define FUSE_OP(a) .a = &tagfs_##a 

static inline char *tagfs_get_db_file() {
	return (char *) fuse_get_context()->private_data ;
}

static void *tagfs_init(struct fuse_conn_info *conn) {
}

static int tagfs_getattr(const char *path, struct stat *stbuf) {
	stbuf->st_uid = getuid() ;
	stbuf->st_gid = getgid() ;
	stbuf->st_blksize = 512 ;

	if(strcmp(path,"/") == 0) {
		stbuf->st_mode = S_IFDIR | 0777 ;
		stbuf->st_blocks = 0 ;
		stbuf->st_nlink = 1 ;
		stbuf->st_ino = 1 ;
		stbuf->st_size = 0 ;
		return 0;
	}
	return -ENOENT ;
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

	fuse_opt_add_arg(&args, argv[0]) ;
	fuse_opt_add_arg(&args, "-o") ;
	fuse_opt_add_arg(&args, "use_ino") ;
	for(i = 1; i < argc-2 ; i++) 
		fuse_opt_add_arg(&args, argv[i]) ;
	if(argc > 2)
		fuse_opt_add_arg(&args, argv[argc-1]) ;


	return fuse_main(args.argc, args.argv, &tagfs_ops, argv[argc-2]) ;
}
