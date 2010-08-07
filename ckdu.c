/* CKDU - C-Kurs clone of du
 *
 * Written by
 *   Sebastian Pipping <sebastian@pipping.org>
 *
 * Licensed under GPL v3 or later
 */

/* GLIBC begin */
#define _GNU_SOURCE  /* for tdestroy */
#include <search.h> /* tfind, tsearch */
/* GLIBC end */

#include <sys/types.h>  /* for opendir, readdir, stat */
#include <sys/stat.h> /* for stat */
#include <dirent.h>  /* for opendir, readdir */
#include <errno.h> /* for errno */

#include <string.h> /* for strlen, strcmp, memcpy */
#include <stdlib.h> /* for malloc, NULL, qsort */
#include <assert.h> /* for assert */
#include <stdio.h> /* for printf, fprintf, sprintf */

typedef int bool;
const bool true = 1;
const bool false = 0;

typedef struct _ckdu_tree_entry {
	/* File/dir/link name (without path!), no more than MAX_NAME+1 bytes in size */
	char *name;

	/* Subset of struct stat filled by stat() */
	dev_t device;
	ino_t inode;
	off_t content_size;
	mode_t mode;

	struct _ckdu_tree_entry *sibling;

	union {
		struct {
			struct _ckdu_tree_entry *child;
			off_t add_content_size;
		} dir;
/*
		struct {
			char *target;
		} link;
*/
	} extra;
} ckdu_tree_entry;

char * strdup(const char *text) {
	char const *source = text ? text : "NULL";
	size_t len = strlen(source);
	char *target = malloc(len + 1);
	if (!target) {
		errno = ENOMEM;
		return NULL;
	}
	memcpy(target, source, len);
	target[len] = '\0';
	return target;
}

char * malloc_path_join(const char *dirname, const char *basename) {
	size_t const len_dirname = strlen(dirname);
	size_t const len_basename = strlen(basename);
	char * const target = malloc(len_dirname + 1 + len_basename + 1);

	memcpy(target, dirname, len_dirname);
	target[len_dirname] = '/';
	memcpy(target + len_dirname + 1, basename, len_basename);
	target[len_dirname + 1 + len_basename] = '\0';

	return target;
}

char * malloc_humanize(off_t int_number) {
	const char * const units[] = {NULL, "  B", "kiB", "MiB", "GiB", "TiB", "PiB", "EiB"};
	off_t divisor = 1024;
	unsigned int exponent = 1;
	double float_number = int_number;
	char * res;

	while (float_number > divisor) {
		float_number /= divisor;
		exponent++;
	}
	assert(exponent < sizeof(units) / sizeof(char *));

	/* Size = Pre-dot + dot + post-dot + unit + \0 */
	res = malloc(4 + 1 + 1 + 3 + 1);
	if (!res) {
		errno = ENOMEM;
		return NULL;
	}

	sprintf(res, "%6.1f%s", float_number, units[exponent]);
	return res;
}

bool is_dir(ckdu_tree_entry const *entry) {
	assert(entry);
	return S_ISDIR(entry->mode);
}

int initialize_tree_entry(ckdu_tree_entry *entry, const char *dirname, const char *basename) {
	char * const path = malloc_path_join(dirname, basename);
	struct stat props;
	int res;
	errno = 0;
	
	res = lstat(path, &props);
	free(path);

	entry->device = props.st_dev;
	entry->inode = props.st_ino;
	entry->content_size = props.st_size;
	entry->mode = props.st_mode;

	entry->name = strdup(basename);
	assert(entry->name);

	entry->extra.dir.child = NULL;
	entry->sibling = NULL;
	entry->extra.dir.add_content_size = 0;
	return res;
}

void default_error(const char ** constant, const char ** description) {
	*constant = "E???";
	*description = "Unknown error";
}

void describe_opendir_error(int code, const char ** constant, const char ** description) {
	/* http://opengroup.org/onlinepubs/007908799/xsh/opendir.html */
	switch (code) {
	case EACCES: *constant = "EACCES"; *description = "Search permission is denied for the component of the path prefix of dirname or read permission is denied for dirname."; return;
	case ELOOP: *constant = "ELOOP"; *description = "Too many symbolic links were encountered in resolving path."; return;
	case ENOENT: *constant = "ENOENT"; *description = "A component of dirname does not name an existing directory or dirname is an empty string."; return;
	case ENOTDIR: *constant = "ENOTDIR"; *description = "A component of dirname is not a directory."; return;
	case EMFILE: *constant = "EMFILE"; *description = "{OPEN_MAX} file descriptors are currently open in the calling process. "; return;
	case ENAMETOOLONG: *constant = "ENAMETOOLONG"; *description = "Pathname resolution of a symbolic link produced an intermediate result whose length exceeds {PATH_MAX}."; return;
	case ENFILE: *constant = "ENFILE"; *description = "Too many files are currently open in the system."; return;
	default: default_error(constant, description); return;
	}
	assert(false);
}

void describe_readdir_error(int code, const char ** constant, const char ** description) {
	/* http://www.opengroup.org/onlinepubs/009695399/functions/readdir.html */
	switch (code) {
	case EOVERFLOW: *constant = "EOVERFLOW"; *description = "One of the values in the structure to be returned cannot be represented correctly."; return;
	case EBADF: *constant = "EBADF"; *description = "The dirp argument does not refer to an open directory stream."; return;
	case ENOENT: *constant = "ENOENT"; *description = "The current position of the directory stream is invalid."; return;
	default: default_error(constant, description); return;
	}
	assert(false);
}

void describe_stat_error(int code, const char ** constant, const char ** description) {
	/* http://www.opengroup.org/onlinepubs/000095399/functions/stat.html */
	switch (code) {
	case EACCES: *constant = "EACCES"; *description = "Search permission is denied for a component of the path prefix."; return;
	case EIO: *constant = "EIO"; *description = "An error occurred while reading from the file system."; return;
	case ENOENT: *constant = "ENOENT"; *description = "A component of path does not name an existing file or path is an empty string."; return;
	case ENOTDIR: *constant = "ENOTDIR"; *description = "A component of the path prefix is not a directory."; return;
	case ELOOP: *constant = "ELOOP"; *description = "More than {SYMLOOP_MAX} symbolic links were encountered during resolution of the path argument."; return;
	case ENAMETOOLONG: *constant = "ENAMETOOLONG"; *description = "As a result of encountering a symbolic link in resolution of the path argument, the length of the substituted pathname string exceeded {PATH_MAX}."; return;
	case EOVERFLOW: *constant = "EOVERFLOW"; *description = "A value to be stored would overflow one of the members of the stat structure. "; return;
	default: default_error(constant, description); return;
	}
	assert(false);
}

void print_error(int code, const char * action, const char *dirname, const char *basename, const char * constant, const char * description) {
	fprintf(stderr, "Error %s(%i) occured when %s \"%s/%s\": %s\n", constant, code, action, dirname, basename ? basename : "", description);
}

void handle_stat_error(int code, const char *dirname, const char *basename) {
	const char * constant = NULL;
	const char * description = NULL;
	describe_stat_error(code, &constant, &description);
	print_error(code, "statting", dirname, basename, constant, description);
}

void handle_readdir_error(int code, const char *dirname) {
	const char * constant = NULL;
	const char * description = NULL;
	describe_readdir_error(code, &constant, &description);
	print_error(code, "reading", dirname, NULL, constant, description);
}

void handle_opendir_error(int code, const char *dirname) {
	const char * constant = NULL;
	const char * description = NULL;
	describe_opendir_error(code, &constant, &description);
	print_error(code, "opening", dirname, NULL, constant, description);
}



int compare_siblings(const void *void_a, const void *void_b) {
	ckdu_tree_entry const * const a = *(ckdu_tree_entry const * const *)void_a;
	ckdu_tree_entry const * const b = *(ckdu_tree_entry const * const *)void_b;

	/* Meant to compare entries as following:
	 * 1. Dirs before files
	 * 2. Big things before small things, content-wise
	 * 3. After that sort alphabetically
	 */
	int const diff_dir = is_dir(b) - is_dir(a);
	if (diff_dir) {
		return diff_dir;
	} else {
		int const diff_size = (b->content_size + b->extra.dir.add_content_size)
				- (a->content_size + a->extra.dir.add_content_size);
		if (diff_size) {
			return diff_size;
		} else {
			return strcmp(a->name, b->name);
		}
	}
}

void sort_siblings(ckdu_tree_entry *parent, int child_count) {
	ckdu_tree_entry ** const array = malloc(child_count * sizeof(ckdu_tree_entry *));
	ckdu_tree_entry *read = parent->extra.dir.child;
	ckdu_tree_entry *prev;
	int i = 0;

	if (!child_count) {
		/* Empty list is always sorted */
		return;
	}

	/* Fill array from linked list */
	for (; i < child_count; i++) {
		assert(read);
		array[i] = read;
		read = read->sibling;
	}

	/* Sort array */
	qsort(array, child_count, sizeof(ckdu_tree_entry *), compare_siblings);

	/* Re-create list from array */
	parent->extra.dir.child = array[0];
	prev = array[0];
	for (i = 1; i < child_count; i++) {
		prev->sibling = array[i];
		prev = array[i];
	}
	prev->sibling = NULL;

	free(array);
}

int compare_trees_id_wise(const void *void_a, const void *void_b) {
	ckdu_tree_entry const * const a = (ckdu_tree_entry const *)void_a;
	ckdu_tree_entry const * const b = (ckdu_tree_entry const *)void_b;

	const int dev_diff = a->device - b->device;
	if (dev_diff) {
		return dev_diff;
	} else {
		return a->inode - b->inode;
	}
}

bool add_to_pool(void **inode_pool, ckdu_tree_entry const *entry) {
	ckdu_tree_entry const * const key = entry;
	ckdu_tree_entry **value;

	value = tfind(key, inode_pool, compare_trees_id_wise);
	if (value) {
		return false;
	}

	value = tsearch(key, inode_pool, compare_trees_id_wise);
	if (!value) {
		/* Ran out of space, TODO */
		assert(false);
	}
	return true;
}

void crawl_tree(ckdu_tree_entry *virtual_root, void **inode_pool, const char *dirname) {
	DIR * dir;
	struct dirent *entry;
	ckdu_tree_entry *prev = NULL;
	int child_count = 0;

	errno = 0;
	dir = opendir(dirname);
	if (!dir) {
		handle_opendir_error(errno, dirname);
		return;
	}

	do {
		errno = 0;
		entry = readdir(dir);
		if (!entry) {
			if (errno) {
				handle_opendir_error(errno, dirname);
			}
		} else {
			if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
				ckdu_tree_entry * const node = malloc(sizeof(ckdu_tree_entry));

				errno = 0;
				if (initialize_tree_entry(node, dirname, entry->d_name)) {
					handle_stat_error(errno, dirname, entry->d_name);
				} else {
					if (prev) {
						prev->sibling = node;
					} else {
						virtual_root->extra.dir.child = node;
					}
					prev = node;
					child_count++;

					if (is_dir(node)) {
						char * const child_dirname = malloc_path_join(dirname, entry->d_name);
						crawl_tree(node, inode_pool, child_dirname);
						free(child_dirname);
					}

					if (add_to_pool(inode_pool, node)) {
						/* Inode not seen in sister trees before */
						virtual_root->extra.dir.add_content_size += node->content_size;
						if (is_dir(node)) {
							virtual_root->extra.dir.add_content_size += node->extra.dir.add_content_size;
						}
					}
				}
			}
		}
	} while (entry);

	sort_siblings(virtual_root, child_count);

	closedir(dir);
}

bool is_boring_folder(const char *basename) {
	char const *blacklist[] = {"autom4te.cache", ".git", ".svn", "CVS"};
	size_t i = 0;
	for (; i < sizeof(blacklist) / sizeof(char *); i++) {
		if (!strcmp(basename, blacklist[i])) {
			return true;
		}
	}
	return false;
}



void present_tree_indent(ckdu_tree_entry const *virtual_root, char const *indent) {
	long const bytes_content = virtual_root->content_size + (is_dir(virtual_root) ? virtual_root->extra.dir.add_content_size : 0);
	ckdu_tree_entry const * child = virtual_root->extra.dir.child;

	char const * const slash_or_not = is_dir(virtual_root) ? "/" : "";
	char * const size_display = malloc_humanize(bytes_content);
	printf("%9s%s %s%s\n", size_display, indent, virtual_root->name, slash_or_not);
	free(size_display);

	if (is_dir(virtual_root) && child) {
		size_t const child_indent_len = strlen(indent) + 2;
		size_t i = 0;
		char * const child_indent = malloc(child_indent_len + 1);

		/* Make indendation */
		for (; i < child_indent_len; i++) {
			child_indent[i] = ' ';
		}
		child_indent[child_indent_len] = '\0';

		/* List children */
		if (is_boring_folder(virtual_root->name)) {
			printf("%9s%s %s\n", "...", child_indent, "...");
		} else {
			do {
				present_tree_indent(child, child_indent);
				child = child->sibling;
			} while (child);
		}

		free(child_indent);
	}
}

void present_tree(ckdu_tree_entry const *virtual_root) {
	present_tree_indent(virtual_root, "");
}

void noop_free(void *key) {
	key = key;
}

int main(int argc, char **argv) {
	ckdu_tree_entry pwd_entry;
	void *inode_pool = NULL;
	const char * const path = (argc > 1) ? argv[1] : ".";

	if (initialize_tree_entry(&pwd_entry, path, ".")) {
		handle_stat_error(errno, path, ".");
		return 1;
	}
	crawl_tree(&pwd_entry, &inode_pool, path);
	present_tree(&pwd_entry);

	tdestroy(inode_pool, noop_free);
	return 0;
}
