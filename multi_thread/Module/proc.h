#ifndef PROC_H
#define PROC_H

int fh_install_hook (struct ftrace_hook *hook);

void fh_remove_hook (struct ftrace_hook *hook);
#endif