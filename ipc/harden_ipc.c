#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/ipc.h>
#include <linux/ipc_namespace.h>
#include <linux/cred.h>

int harden_ipc __read_mostly = IS_ENABLED(CONFIG_SECURITY_HARDEN_IPC);

int
ipc_permitted(struct ipc_namespace *ns, struct kern_ipc_perm *ipcp, int requested_mode, int granted_mode)
{
	int write;
	int orig_granted_mode;
	kuid_t euid;
	kgid_t egid;

	if (!harden_ipc)
		return 1;

	euid = current_euid();
	egid = current_egid();

	write = requested_mode & 00002;
	orig_granted_mode = ipcp->mode;

	if (uid_eq(euid, ipcp->cuid) || uid_eq(euid, ipcp->uid))
		orig_granted_mode >>= 6;
	else {
		/* if likely wrong permissions, lock to user */
		if (orig_granted_mode & 0007)
			orig_granted_mode = 0;
		/* otherwise do a egid-only check */
		else if (gid_eq(egid, ipcp->cgid) || gid_eq(egid, ipcp->gid))
			orig_granted_mode >>= 3;
		/* otherwise, no access */
		else
			orig_granted_mode = 0;
	}
	if (!(requested_mode & ~granted_mode & 0007) && (requested_mode & ~orig_granted_mode & 0007) &&
	    !ns_capable_noaudit(ns->user_ns, CAP_IPC_OWNER)) {
		return 0;
	}
	return 1;
}
