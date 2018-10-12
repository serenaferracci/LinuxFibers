#include <linux/ftrace.h>

#include "proc.h"

static int resolve_hook_address (struct ftrace_hook *hook){
	hook->address = kallsyms_lookup_name(hook->name);
	if (!hook->address) {
		pr_debug("unresolved symbol: %s\n", hook->name);
		return -ENOENT;
	}
	*((unsigned long*) hook->original) = hook->address;
	return 0;
}

int fh_install_hook (struct ftrace_hook *hook){
	int err;
	err = resolve_hook_address(hook);
	if (err) return err;

	hook->ops.func = fh_ftrace_thunk;
	hook->ops.flags = FTRACE_OPS_FL_SAVE_REGS
					| FTRACE_OPS_FL_IPMODIFY;

	err = ftrace_set_filter_ip(&hook->ops, hook->address, 0, 0);
	if (err) {
		pr_debug("ftrace_set_filter_ip() failed: %d\n", err);
		return err;
	}
	err = register_ftrace_function(&hook->ops);
	if (err) {
		pr_debug("register_ftrace_function() failed: %d\n", err);
		//turn off ftrace in case of an error
		ftrace_set_filter_ip(&hook->ops, hook->address, 1, 0); 
		return err;
	}
	return 0;
}

void fh_remove_hook (struct ftrace_hook *hook){
	int err;

	err = unregister_ftrace_function(&hook->ops);
	if (err) pr_debug("unregister_ftrace_function() failed: %d\n", err);

	err = ftrace_set_filter_ip(&hook->ops, hook->address, 1, 0);
	if (err) pr_debug("ftrace_set_filter_ip() failed: %d\n", err);
}


static void notrace fh_ftrace_thunk (unsigned long ip, unsigned long parent_ip,
                struct ftrace_ops *ops, struct pt_regs *regs){
	struct ftrace_hook *hook = container_of(ops, struct ftrace_hook, ops);
	/* Skip the function calls from the current module. */
	if (!within_module(parent_ip, THIS_MODULE)) regs->ip = (unsigned long) hook->function;
}