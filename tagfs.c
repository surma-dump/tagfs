#define FUSE_USE_VERSION 26
#include <stdlib.h> // getuid() getgid()
#include <sys/types.h>

#include <fuse.h> // FUSE... duh
#include <db.h> // BerkeleyDB
#include <errno.h> // Return values
#include <time.h> // time() 
#include <string.h> // memset(), basename()

#define FUSE_OP(a) .a = &tagfs_##a 

DB *db ;

struct filekey {
	short int name_length ;
	char *filename ;
}

struct file {
	struct stat stats ;
	char *data ;
}

static inline char *tagfs_get_db_file() {
	return (char *) fuse_get_context()->private_data ;
}

static void *tagfs_init(struct fuse_conn_info *conn) {
	db_create(&db, NULL, 0) ;
	db->open(db, NULL, tagfs_get_db_file(), "files", DB_BTREE, DB_CREATE, 0700) ;
}

static void tagfs_destroy() {
	db->close(db, 0) ;
}

static int tagfs_getattr(const char *path, struct stat *stbuf) {
	char fname[256] ;
	DBT key, value ;



	stbuf->st_uid = getuid() ;
	stbuf->st_gid = getgid() ;
	stbuf->st_blksize = 512 ;
	stbuf->st_blocks = 0 ;
	stbuf->st_nlink = 1 ;
	stbuf->st_ino = 1 ;
	stbuf->st_size = 0 ;

	if(strcmp(path,"/") == 0) { 
		stbuf->st_mode = S_IFDIR | 0777 ;
		return 0 ;
	}

	memset(&key, 0, sizeof(DBT)) ;
	memset(&value, 0, sizeof(DBT)) ;
	memset(fname, 0, 256) ;
	strncpy(fname, (char *)basename(path), 256) ;

	key.data = fname ;
	key.size = 256 ;

	if(db->get(db, NULL, &key, &value, 0) != 0)
		return -ENOENT ;

	stbuf->st_mode = S_IFREG | 0777 ;
	stbuf->st_mtime = *(int*)value.data ;
	return 0;
}

static int tagfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi) {
	DBC *cp ;
	DBT key, value ;

	db->cursor(db, NULL, &cp, 0) ;

	memset(&key, 0, sizeof(DBT)) ;
	memset(&value, 0, sizeof(DBT)) ;

	filler(buf, ".", NULL, 0) ;
	filler(buf, "..", NULL, 0) ;
	
	while(cp->get(cp, &key, &value, DB_NEXT) == 0) 
		filler(buf, (char *) (key.data), NULL, 0) ;
	cp->close(cp) ;
	return 0 ;
}

static int tagfs_mknod(const char *path, mode_t mode, dev_t dev) {
	char fname[256] ;
	int t = (int) time(NULL) ;
	DBT key, value ;

	memset(&key, 0, sizeof(DBT)) ;
	memset(&value, 0, sizeof(DBT)) ;
	memset(fname, 0, 256) ;

	strncpy(fname, (char *)basename(path), 256) ;
	fprintf(stderr, "\n===> %s\n", (char *)basename(path)) ;
	key.data = fname ;
	key.size = 256 ;

	value.data = &t ;
	value.size = sizeof(t) ;

	db->put(db, NULL, &key, &value, 0) ;
	return 0 ;
}

static struct fuse_operations tagfs_ops = {
	FUSE_OP(init),
	FUSE_OP(destroy),
	FUSE_OP(getattr),
	FUSE_OP(mknod),
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
