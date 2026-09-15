
#define _GNU_SOURCE
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>
#include <dlfcn.h>
static void trace(int sig, siginfo_t *info, void *ctx) {
 void *frames[40]; int n=backtrace(frames,40); backtrace_symbols_fd(frames,n,2); _exit(128+sig);
}
int sigaction(int sig, const struct sigaction *act, struct sigaction *old) {
 static int (*real_sa)(int,const struct sigaction *,struct sigaction *);
 if (!real_sa) real_sa=dlsym(RTLD_NEXT,"sigaction");
 if (sig==SIGSEGV && act) { struct sigaction a=*act; a.sa_sigaction=trace; a.sa_flags|=SA_SIGINFO; return real_sa(sig,&a,old); }
 return real_sa(sig,act,old);
}
