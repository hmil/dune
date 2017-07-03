#include <linux/coredump.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/regset.h>
#include <linux/elf.h>

#include "vmx.h"
#include "dune.h"

#if defined(DO_COREDUMP)
typedef long (*do_coredump_hack) (struct siginfo *siginfo);
static do_coredump_hack dune_do_coredump = (do_coredump_hack) DO_COREDUMP;
#endif

#if defined(TASK_USER_REGSET_VIEW)
typedef struct user_regset_view* (*task_user_regset_view_hack) (struct task_struct *task);
static task_user_regset_view_hack dune_task_user_regset_view = (task_user_regset_view_hack) TASK_USER_REGSET_VIEW;
#endif


long dune_dump_core(const struct dune_config *conf)
{
	struct siginfo info;
	struct thread_struct *host_state;
	unsigned long sp;
	long ret = -1;

	const struct user_regset_view *view;



	// if (conf->vcpu == NULL) {
	//	 printk(KERN_ERR "Dune config vcpu is NULL, cannot dump core");
	//	 return -1;
	// }

	printk(KERN_INFO "Kern stack is %016lx\n", current->stack);

	host_state = kmalloc(sizeof(struct thread_struct), GFP_KERNEL);
	if (!host_state) {
		return -1;
	}

	memset(&info, 0, sizeof(struct siginfo));
	info.si_signo = SIGUSR1;
	info.si_code = SI_QUEUE;
	info.si_int = 0;

	// printk(KERN_INFO "vcpu is: %016lx\n", conf->vcpu);

	printk(KERN_INFO "Saving host thread state\n");
	memcpy(host_state, &current->thread, sizeof(*host_state));

	vmx_dump_vcpu_to_user_thread(&current->thread);

	if (dune_task_user_regset_view != (void *)0xffffffff8103d330LU) {
		printk(KERN_INFO "Invalid task_user_regset_view address: %016lx\n", dune_task_user_regset_view);
		goto fail;
	}

	printk(KERN_INFO "Getting regset view\n");
	view = dune_task_user_regset_view(current);
	printk(KERN_INFO "Ready to alter user regset view\n");

	if (view == NULL) {
		printk(KERN_ERR "Regset view is NULL! cannot dump core!\n");
		goto fail;
	}
	if (view->n == 0) {
		printk(KERN_ERR "Regset view has no entries, cannot dump core!\n");
		goto fail;
	}

	if (view->regsets[0].core_note_type != NT_PRSTATUS) {
		printk(KERN_ERR "Regset view has unexpected structure, cannot dump core!\n");
		goto fail;
	}
	

	view->regsets[0].get(current, &view->regsets[0], 
					offsetof(struct user_regs_struct, sp), view->regsets[0].size,
					&sp, NULL);

	printk(KERN_INFO "Greg stack pointer is  %016lx\n", sp);
	printk(KERN_INFO "And we replace it with %016lx\n", current->thread.sp0);
	view->regsets[0].set(current, &view->regsets[0], 
					offsetof(struct user_regs_struct, sp), view->regsets[0].size,
					&current->thread.sp0, NULL);

	printk(KERN_INFO "About to dump core\n");
	dune_do_coredump(&info);
	printk(KERN_INFO "Core dumped\n");

	printk(KERN_INFO "Now restoring greg stack pointer\n");
	view->regsets[0].set(current, &view->regsets[0], 
					offsetof(struct user_regs_struct, sp), view->regsets[0].size,
					&sp, NULL);

	printk(KERN_INFO "Restoring host thread state\n");
	memcpy(&current->thread, host_state, sizeof(*host_state));

	ret = 0;

fail:
	kfree(host_state);

	return ret;
}