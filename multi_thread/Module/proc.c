#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/sched/task_stack.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <asm/fpu/internal.h>
#include <linux/kprobes.h>
#include "fiber_ioctl.h"

int Pre_Handler_Proc(struct kprobe *p, struct pt_regs *regs){ 
	printk("Attivato proc_pident_readdir\n")
	return 0;
} 