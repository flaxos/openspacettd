/** @file blueprint_io_faults.c Test-only interposer for Blueprint storage failure injection. */
/* Test-only Linux interposer. Built in a temporary directory by the fault runner;
 * never linked into the game. It fails one armed Blueprint temporary-file call. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int armed, hits, partial_write;
static unsigned char temporary[4096];
void BlueprintTestFaultArm(int enable) { armed = enable; if (enable) { hits = 0; partial_write = 0; } }
int BlueprintTestFaultHits(void) { return hits; }
static int fail(const char *op) {
	const char *wanted = getenv("OST_BP_FAULT_OP");
	if (!armed || hits || !wanted || strcmp(wanted, op)) return 0;
	++hits; errno = EIO; return 1;
}
int open(const char *path, int flags, ...) {
	int mode = 0;
	if (flags & O_CREAT) { va_list args; va_start(args, flags); mode = va_arg(args, int); va_end(args); }
	int (*real)(const char*, int, ...) = dlsym(RTLD_NEXT, "open");
	if (strstr(path, ".tmp-") && fail("open")) return -1;
	int fd = real(path, flags, mode);
	if (fd >= 0 && fd < 4096) temporary[fd] = strstr(path, ".tmp-") != NULL;
	return fd;
}
ssize_t write(int fd, const void *buf, size_t count) {
	ssize_t (*real)(int,const void*,size_t) = dlsym(RTLD_NEXT,"write");
	if (fd >= 0 && fd < 4096 && temporary[fd]) {
		const char *wanted = getenv("OST_BP_FAULT_OP");
		if (armed && !partial_write && wanted && !strcmp(wanted, "write") && count > 1) {
			partial_write = 1;
			return real(fd,buf,count / 2); /* Write actual partial contents before failing the next call. */
		}
		if (fail("write")) return -1;
	}
	return real(fd,buf,count);
}
int fsync(int fd) {
	int (*real)(int) = dlsym(RTLD_NEXT,"fsync");
	if (fd >= 0 && fd < 4096 && temporary[fd] && fail("fsync")) return -1;
	return real(fd);
}
int close(int fd) {
	int (*real)(int) = dlsym(RTLD_NEXT,"close");
	int injected = fd >= 0 && fd < 4096 && temporary[fd] && fail("close");
	if (fd >= 0 && fd < 4096) temporary[fd] = 0;
	int result = real(fd);
	if (injected) { errno = EIO; return -1; }
	return result;
}
int rename(const char *old, const char *new_path) {
	int (*real)(const char*,const char*) = dlsym(RTLD_NEXT,"rename");
	if (strstr(old,".tmp-") && fail("rename")) return -1;
	return real(old,new_path);
}
int link(const char *old, const char *new_path) {
	int (*real)(const char*,const char*) = dlsym(RTLD_NEXT,"link");
	if (strstr(old,".tmp-") && fail("link")) return -1;
	return real(old,new_path);
}
