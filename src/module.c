
#define _XOPEN_SOURCE 500
#define _XOPEN_SOURCE_EXTENDED

#define PAM_SM_AUTH
#include <security/pam_modules.h>
#include <security/_pam_macros.h>

#include "pam-phy-token.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mount.h>


PAM_EXTERN
int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    /* This is the user which wants to authenticate. Make sure it's valid. */
    const char *user;
    if (pam_get_user(pamh, &user, NULL) != PAM_SUCCESS || !user || !*user)
        return PAM_AUTH_ERR;

    /* Initialize the module. Boooring. */
    struct pam_phy_module module;
    if (pam_phy_module_init(&module, user))
        return PAM_AUTH_ERR;

    /* Default is failure. */
    int ret = PAM_AUTH_ERR;

    /* Iterate over all devices, try to authenticate against one. */
    for (int i = 0; i < module.count; ++i) {
        if (!pam_phy_auth(&module, &module.device[i])) {
            ret = PAM_SUCCESS;
            goto out;
        }
    }

out:
    /* Cleanup, we don't want to leave a mess behind! */
    pam_phy_module_cleanup(&module);

    return ret;
}

PAM_EXTERN
int pam_sm_setcred(pam_handle_t *pamh,int flags,int argc, const char **argv)
{
    return PAM_SUCCESS;
}
