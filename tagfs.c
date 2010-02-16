#define FUSE_USE_VERSION 26
#include <stdlib.h> // getuid() getgid()
#include <sys/types.h>
#include <signal.h> // getpid(), kill()
#include <fuse.h> // FUSE... duh
#include <db.h> // BerkeleyDB
#include <errno.h> // Return values
#include <time.h> // time() 
#include <string.h> // memset(), basename(), memcpy() 
#include <stddef.h> // offsetof()

// Helper Macros
#define FUSE_OP(a) .a = &tagfs_##a 
#define TAGFS_OPT(n, p, v) {n, offsetof(struct tagfs_options, p), v}
#define KEYSIZE(a) sizeof(struct filekey) + a->name_length


//Typedefs
enum {	// Values for option parser
	KEY_INIT,
	KEY_HELP
} ;

enum { // Values for options
	TAGFS_INIT
} ;

struct tagfs_options {
	char *db_file ;
	int db_flags ;
} ;

struct filekey {
	unsigned short int name_length ;
	char *filename ;
};

struct file {
	struct stat stats ;
	char *data ;
};

// Prototypes 
static inline char *tagfs_get_db_file() ;
static inline int tagfs_get_db_flags() ;
static inline int tagfs_get_pid() ;
static inline int tagfs_get_gid() ;
static inline int tagfs_get_uid() ;
static void *tagfs_init(struct fuse_conn_info *conn) ;
static void tagfs_destroy() ;
struct filekey *generate_filekey(char *path) ;
static int tagfs_getattr(const char *path, struct stat *stbuf) ;
static int tagfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi) ;
static int tagfs_mknod(const char *path, mode_t mode, dev_t dev) ;
static int tagfs_fsync(const char *path, int dontknow, struct fuse_file_info *fi) ;
static int tagfs_optionparser(void *data, const char *arg, int key,
		struct fuse_args *outargs) ;
int main(int argc, char **argv) ;


// Globals
DB *db_files, *db_f2t, *db_tags, *db_t2f ;

static struct fuse_operations tagfs_ops = {
	FUSE_OP(init),
	FUSE_OP(destroy),
	FUSE_OP(getattr),
	FUSE_OP(mknod),
	FUSE_OP(fsync),
	FUSE_OP(readdir)
} ;

static struct fuse_opt opts[] = {
	TAGFS_OPT("init", db_flags, TAGFS_INIT),
	TAGFS_OPT("-b %s", db_file, 0),
	TAGFS_OPT("--dbfile=%s", db_file, 0),

	FUSE_OPT_KEY("-h", KEY_HELP),
	FUSE_OPT_KEY("--help", KEY_HELP),
	FUSE_OPT_END

} ;

// Functions
static inline char *tagfs_get_db_file() {
	return ((struct tagfs_options*) fuse_get_context()->private_data)->db_file ;
}

static inline int tagfs_get_db_flags() {
	return ((struct tagfs_options*) fuse_get_context()->private_data)->db_flags ;
}

static inline int tagfs_get_uid() {
	return fuse_get_context()->uid ;
}

static inline int tagfs_get_gid() {
	return fuse_get_context()->gid ;
}

static inline int tagfs_get_pid() {
	return fuse_get_context()->pid ;
}

static inline struct fuse *tagfs_get_fuse_handle() {
	return fuse_get_context()->fuse ;
}

static void *tagfs_init(struct fuse_conn_info *conn) {
	int flags = 0;
	int error ;

	if(tagfs_get_db_file() == NULL) {
		fprintf(stderr, "tagfs: No db file specified, use -b\n") ;
		fuse_exit(tagfs_get_fuse_handle()) ;
		return ;
	}

	if(tagfs_get_db_flags() == TAGFS_INIT)
		flags = DB_CREATE | DB_EXCL ;

	db_create(&db_files, NULL, 0) ;
	db_create(&db_f2t,   NULL, 0) ;
	db_create(&db_tags,  NULL, 0) ;
	db_create(&db_t2f,   NULL, 0) ;
	error  = db_files->open(db_files, NULL, tagfs_get_db_file(), "files", 
			DB_BTREE, flags, 0700) ;
	error |= db_f2t->open  (db_f2t,   NULL, tagfs_get_db_file(), "f2t",   
			DB_BTREE, flags, 0700) ;
	error |= db_tags->open (db_tags,  NULL, tagfs_get_db_file(), "tags",  
			DB_BTREE, flags, 0700) ;
	error |= db_t2f->open  (db_t2f,   NULL, tagfs_get_db_file(), "t2f",   
			DB_BTREE, flags, 0700) ;

	if(error != 0) {
		if(tagfs_get_db_flags() == TAGFS_INIT) 
			fprintf(stderr, "tagfs: init: Could not create databases\n") ;
		else
			fprintf(stderr, "tagfs: init: Could not open databases\n") ;
		fuse_exit(tagfs_get_fuse_handle()) ;
		return ;
	}

	return ;
}

static void tagfs_destroy() {
	db_files->sync(db_files, 0) ;
	db_f2t->sync  (db_f2t,   0) ;
	db_tags->sync (db_tags,  0) ;
	db_t2f->sync  (db_t2f,   0) ;

	db_files->close(db_files, 0) ;
	db_f2t->close  (db_f2t,   0) ;
	db_tags->close (db_tags,  0) ;
	db_t2f->close  (db_t2f,   0) ;
}

struct filekey *generate_filekey(char *path) {
	unsigned short int len = strlen(path) ;
	struct filekey *p = (struct filekey *) malloc (sizeof(struct filekey) 
		+ len + 1) ;
	memset(p, 0, sizeof(struct filekey) + len + 1) ;
	p->name_length = len ;
	p->filename = (char*)p + 1 ;
	strncpy(p->filename, path, len) ;
	fprintf(stderr, "filekey: \"%s\"|%d=>\"%s\"|%d\n", path, len, p->filename, p->name_length) ;
	return p ;
}


static int tagfs_getattr(const char *path, struct stat *stbuf) {
	DBT db_key, db_value ;
	struct filekey *f_key ;
	struct file *f ;
	
	if(strcmp("/", path) == 0) {
		stbuf->st_mode = S_IFDIR | 0755 ;
		stbuf->st_nlink = 1 ;
		stbuf->st_uid = tagfs_get_uid() ;
		stbuf->st_gid = tagfs_get_gid() ;
		stbuf->st_mtime = time(NULL) ;
		return 0 ;
	}

	memset(&db_key, 0, sizeof(DBT)) ;
	memset(&db_value, 0, sizeof(DBT)) ;

	f_key = generate_filekey((char *)basename(path)) ;	
	db_key.data = f_key ;
	db_key.size = KEYSIZE(f_key) ;

	if(db_files->get(db_files, NULL, &db_key, &db_value, 0) != 0)
		return -ENOENT ;
	free(f_key) ;
	fprintf(stderr, "====> %d\n", ((struct file*)db_value.data)->stats.st_ino) ;
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
	fprintf(stderr, "====> %d\n", f.stats.st_ino) ;
	f.stats.st_mode = mode ;
	f.stats.st_nlink = 1 ;
	f.stats.st_uid = tagfs_get_uid(); 
	f.stats.st_gid = tagfs_get_gid();
	f.stats.st_size = 0 ;
	f.stats.st_atime = f.stats.st_ino ;
	f.stats.st_ctime = f.stats.st_ino ;
	f.stats.st_mtime = f.stats.st_ino ;

	db_files->put(db_files, NULL, &db_key, &db_value, 0) ;
	free(f_key) ;
	return 0 ;
}

static int tagfs_fsync(const char *path, int dontknow, struct fuse_file_info *fi) {
	db_files->sync(db_files, 0) ;
	db_f2t->sync  (db_f2t,   0) ;
	db_tags->sync (db_tags,  0) ;
	db_t2f->sync  (db_t2f,   0) ;
}

static int tagfs_optionparser(void *data, const char *arg, int key,
		struct fuse_args *outargs) {
	switch(key) {
		case KEY_HELP:
			fprintf(stderr, "Usage: %s [options] <mountpoint>\n"
					"\n"
					"\t-b,--dbfile  Database File\n"
					"\t-o init      Initialize new database\n"
					"\n"
					, outargs->argv[0]) ;
			fuse_opt_add_arg(outargs, "-ho") ;
			fuse_main(outargs->argc, outargs->argv, &tagfs_ops, NULL) ;
			exit(0) ;
	}
	return 1 ;
}

int main(int argc, char **argv) {
	int i ;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv) ;
	struct tagfs_options tfo = {
		.db_file = NULL,
		.db_flags = 0 
	};

	fuse_opt_parse(&args, &tfo, opts, tagfs_optionparser) ;

	return fuse_main(args.argc, args.argv, &tagfs_ops, &tfo) ;
}
