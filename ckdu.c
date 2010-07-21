/* CKDU - C-Kurs clone of du
 *
 * Written by
 *   Sebastian Pipping <sebastian@pipping.org>
 *
 * Licensed under GPL v3 or later
 */

#include <sys/types.h>  /* for opendir, readdir, stat */
#include <sys/stat.h> /* for stat */
#include <dirent.h>  /* for opendir, readdir */
#include <errno.h> /* for errno */

#include <string.h> /* for strlen, strcmp, memcpy */
#include <stdlib.h> /* for malloc, NULL, qsort */
#include <assert.h> /* for assert */
#include <stdio.h> /* for printf, fprintf */

typedef int bool;
const bool true = 1;
const bool false = 0;

typedef enum _ckdu_file_type {
	CKDU_FILE_TYPE_FILE,
	CKDU_FILE_TYPE_DIR,
	CKDU_FILE_TYPE_LINK,
	CKDU_FILE_TYPE_OTHER
} ckdu_file_type;

typedef struct _ckdu_tree_entry {
	/* File/dir/link name (without path!), no more than MAX_NAME+1 bytes in size */
	char *name;

	/* Output of stat/fstat/lstat */
	struct stat props;

	struct _ckdu_tree_entry *sibling;

	union {
		struct {
			struct _ckdu_tree_entry *child;
			off_t add_st_size;
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

bool is_dir(ckdu_tree_entry const *entry) {
	assert(entry);
	return S_ISDIR(entry->props.st_mode);
}

int initialize_tree_entry(ckdu_tree_entry *entry, const char *dirname, const char *basename) {
	char * const path = malloc_path_join(dirname, basename);
	int const res = stat(path, &(entry->props));
	free(path);
	entry->name = strdup(basename);
	assert(entry->name);

	entry->extra.dir.child = NULL;
	entry->sibling = NULL;
	entry->extra.dir.add_st_size = 0;
	return res;
}

void fallback_handler(const char *function_name, int code) {
	fprintf(stderr, "A call to %s() produced error code %i.\n", function_name, code);
}

void handle_stat_error(int code) {
	fallback_handler("stat", code);
}

void handle_readdir_error(int code) {
	fallback_handler("readdir", code);
}

void handle_opendir_error(int code) {
	fallback_handler("opendir", code);
}

typedef struct _inode_pool_class {
	int dummy;
	/* TODO */
} inode_pool_class;

void initialize_inode_pool(inode_pool_class *inode_pool) {
	inode_pool = inode_pool; /* TODO */
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
		int const diff_size = (b->props.st_size + b->extra.dir.add_st_size)
				- (a->props.st_size + a->extra.dir.add_st_size);
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

void crawl_tree(ckdu_tree_entry *virtual_root, inode_pool_class *inode_pool, const char *dirname) {
	DIR * dir;
	struct dirent *entry;
	ckdu_tree_entry *prev = NULL;
	int child_count = 0;

	errno = 0;
	dir = opendir(dirname);
	if (!dir) {
		handle_opendir_error(errno);
		return;
	}

	do {
		errno = 0;
		entry = readdir(dir);
		if (!entry) {
			if (errno) {
				handle_opendir_error(errno);
			}
		} else {
			if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
				ckdu_tree_entry * const node = malloc(sizeof(ckdu_tree_entry));

				errno = 0;
				if (initialize_tree_entry(node, dirname, entry->d_name)) {
					handle_stat_error(errno);
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

					virtual_root->extra.dir.add_st_size += node->props.st_size;
					if (is_dir(node)) {
						virtual_root->extra.dir.add_st_size += node->extra.dir.add_st_size;
					}
				}
			}
		}
	} while (entry);

	sort_siblings(virtual_root, child_count);

	closedir(dir);
}

void present_tree_indent(ckdu_tree_entry const *virtual_root, char const *indent) {
	long const bytes_content = virtual_root->props.st_size + (is_dir(virtual_root) ? virtual_root->extra.dir.add_st_size : 0);
	ckdu_tree_entry const * const sibling = virtual_root->sibling;
	ckdu_tree_entry const * const child = virtual_root->extra.dir.child;

	char * const final_name = is_dir(virtual_root) ? malloc_path_join(virtual_root->name, "") : strdup(virtual_root->name);
	printf("%s%-20s    %6li Bytes\n", indent, final_name, bytes_content);
	free(final_name);

	if (is_dir(virtual_root)) {
		size_t const child_indent_len = strlen(indent) + 2;
		size_t i = 0;
		char * const child_indent = malloc(child_indent_len + 1);

		for (; i < child_indent_len; i++) {
			child_indent[i] = ' ';
		}
		child_indent[child_indent_len] = '\0';

		if (child != NULL) {
			present_tree_indent(child, child_indent);
		}

		free(child_indent);
	}

	if (sibling != NULL) {
		present_tree_indent(sibling, indent);
	}
}

void present_tree(ckdu_tree_entry const *virtual_root) {
	present_tree_indent(virtual_root, "");
}

int main(int argc, char **argv) {
	ckdu_tree_entry pwd_entry;
	inode_pool_class inode_pool;
	const char * const path = (argc > 1) ? argv[1] : ".";

	initialize_tree_entry(&pwd_entry, path, ".");
	initialize_inode_pool(&inode_pool);
	crawl_tree(&pwd_entry, &inode_pool, path);
	present_tree(&pwd_entry);

	return 0;
}
