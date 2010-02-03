#define FUSE_USE_VERSION 26
#include <stdlib.h> // getuid() getgid()
#include <sys/types.h>
#include <signal.h> // getpid(), kill()
#include <fuse.h> // FUSE... duh
#include <db.h> // BerkeleyDB
#include <errno.h> // Return values
#include <time.h> // time() 
#include <string.h> // memset(), basename(), memcpy() 

#define FUSE_OP(a) .a = &tagfs_##a 
#define KEYSIZE(a) sizeof(struct filekey) + a->name_length


DB *db_files, *db_f2t, *db_tags, *db_t2f ;

struct filekey {
	short int name_length ;
	char *filename ;
};

struct file {
	struct stat stats ;
	char *data ;
};

static inline char *tagfs_get_db_file() {
	return (char *) fuse_get_context()->private_data ;
}

static void *tagfs_init(struct fuse_conn_info *conn) {
	int error ;
	db_create(&db_files, NULL, 0) ;
	db_create(&db_f2t,   NULL, 0) ;
	db_create(&db_tags,  NULL, 0) ;
	db_create(&db_t2f,   NULL, 0) ;
	error  = db_files->open(db_files, NULL, tagfs_get_db_file(), "files", DB_BTREE, 0, 0700) ;
	error |= db_f2t->open  (db_f2t,   NULL, tagfs_get_db_file(), "f2t",   DB_BTREE, 0, 0700) ;
	error |= db_tags->open (db_tags,  NULL, tagfs_get_db_file(), "tags",  DB_BTREE, 0, 0700) ;
	error |= db_t2f->open  (db_t2f,   NULL, tagfs_get_db_file(), "t2f",   DB_BTREE, 0, 0700) ;

	if(error != 0) {
		fprintf(stderr, "tagfs: init: Could not open databases\n") ;
		kill(getpid(), SIGINT) ;
	}

}

static void tagfs_destroy() {
	db_files->close(db_files, 0) ;
	db_f2t->close  (db_f2t,   0) ;
	db_tags->close (db_tags,  0) ;
	db_t2f->close  (db_t2f,   0) ;
}

struct filekey *generate_filekey(char *path) {
	int len = strlen(path) ;
	struct filekey *p = (struct filekey *) malloc (sizeof(struct filekey) 
		+ len + 1) ;
	p->name_length = len ;
	p->filename = (char*)p + 1 ;
	strncpy(p->filename, path, len) ;
	return p ;
}


static int tagfs_getattr(const char *path, struct stat *stbuf) {
	DBT db_key, db_value ;
	struct filekey *f_key ;
	struct file *f ;
		
	memset(&db_key, 0, sizeof(DBT)) ;
	memset(&db_value, 0, sizeof(DBT)) ;

	f_key = generate_filekey((char *)basename(path)) ;	
	db_key.data = f_key ;
	db_key.size = KEYSIZE(f_key) ;

	if(db_files->get(db_files, NULL, &db_key, &db_value, 0) != 0)
		return -ENOENT ;
	free(f_key) ;
	memcpy(stbuf, &((struct file*)(db_value.data))->stats, sizeof(struct stat)) ;

	return 0;
}

static int tagfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi) {
	DBC *db_cp ;
	DBT db_key, db_value ;

	db_files->cursor(db_files, NULL, &db_cp, 0) ;

	memset(&db_key, 0, sizeof(DBT)) ;
	memset(&db_value, 0, sizeof(DBT)) ;

	filler(buf, ".", NULL, 0) ;
	filler(buf, "..", NULL, 0) ;
	
	while(db_cp->get(db_cp, &db_key, &db_value, DB_NEXT) == 0) 
		filler(buf, ((struct filekey *)(db_key.data))->filename, NULL, 0) ;
	db_cp->close(db_cp) ;
	return 0 ;
}

static int tagfs_mknod(const char *path, mode_t mode, dev_t dev) {
	DBT db_key, db_value ;
	struct filekey *f_key ;
	struct file f ;

	memset(&db_key, 0, sizeof(DBT)) ;
	memset(&db_value, 0, sizeof(DBT)) ;
	memset(&f, 0, sizeof(struct file)) ;

	f_key = generate_filekey((char *)basename(path)) ;
	
	db_key.data = f_key ;
	db_key.size = KEYSIZE(f_key) ;

	db_value.data = &f ;
	db_value.size = sizeof(struct file) ;

	f.stats.st_ino = time(NULL) ;
	f.stats.st_mode = mode ;
	f.stats.st_nlink = 1 ;
	f.stats.st_uid = getuid() ;
	f.stats.st_gid = getgid() ;
	f.stats.st_size = 0 ;
	f.stats.st_atime = f.stats.st_ino ;
	f.stats.st_ctime = f.stats.st_ino ;
	f.stats.st_mtime = f.stats.st_ino ;

	db_files->put(db_files, NULL, &db_key, &db_value, 0) ;
	free(f_key) ;
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
