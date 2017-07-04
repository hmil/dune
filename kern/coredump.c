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

/**
 * Creates a core dump of the virtualized process by leveraging the 
 * linux kernel's do_coredump function.
 */
long dune_dump_core(const struct dune_config *conf)
{
	struct siginfo info;
	__u64 sp;
	__u64 vm_sp;
	const struct user_regset_view *view;


	// Registers information used by the kernel core dumping procedure.
	// We will want to change those such that they reflect the values
	// from the virtualized process and not the ones from the Dune host.
	view = dune_task_user_regset_view(current);

	if (view == NULL) {
		printk(KERN_ERR "Regset view is NULL! cannot dump core!\n");
		return 1;
	}
	if (view->n == 0) {
		printk(KERN_ERR "Regset view has no entries, cannot dump core!\n");
		return 1;
	}

	if (view->regsets[0].core_note_type != NT_PRSTATUS) {
		printk(KERN_ERR "Regset view has unexpected structure, cannot dump core!\n");
		return 1;
	}

	// Backup the real stack pointer
	view->regsets[0].get(current, &view->regsets[0], 
					offsetof(struct user_regs_struct, sp), view->regsets[0].size,
					&sp, NULL);
	
	// Set the virtual machine stack pointer in the regset
	// TODO: We might want to dump the other registers as well
	vm_sp = vmx_get_sp();
	view->regsets[0].set(current, &view->regsets[0], 
					offsetof(struct user_regs_struct, sp), view->regsets[0].size,
					&vm_sp, NULL);

	// do_coredump requires a siginfo parameter so we hack together a fake signal to feed it.
	memset(&info, 0, sizeof(struct siginfo));
	info.si_signo = SIGUSR1;
	info.si_code = SI_QUEUE;
	info.si_int = 0;

	dune_do_coredump(&info);

	// Restore the original stack pointer
	view->regsets[0].set(current, &view->regsets[0], 
					offsetof(struct user_regs_struct, sp), view->regsets[0].size,
					&sp, NULL);

	printk(KERN_INFO "Core dumped\n");

	return 0;
}