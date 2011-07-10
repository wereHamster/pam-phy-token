
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


static int auth(struct pam_phy_module *module, struct pam_phy_device *device)
{
    /* Try to mount the device by its UUID. */
    const char *target = uuidmount(module, __uuid(device->uuid));
    if (!target)
        return 1;

    /* This is where the token is stored. */
    char path[1024];
    snprintf(path, 1024, "%s/.pam-phy-token/%s@%s", target, module->user, module->node);

    /* Open the file in read-write mode, because we'll eventually have to
     * update it. */
    int ret = 1, fd = open(path, O_RDWR);
    if (fd >= 0) {
        /* Read the token. */
        struct token token;
        if (read(fd, &token, sizeof(token)) != sizeof(token))
            goto out;

        /* Compare the token with what is on the host. */
        ret = memcmp(&token, &device->token, sizeof(token));

        /* If the tokens are equal generate a new token. */
        if (!ret) {
            /* Rewind the file descriptor and truncate the file. */
            lseek(fd, SEEK_SET, 0);
            ftruncate(fd, sizeof(token));

            /* Generate the token. */
            mktoken(&token);

            /* Write the file to the device and update the token on the host. */
            if (write(fd, &token, sizeof(token)) == sizeof(token) && !fsync(fd))
                memcpy(&module->device[0].token, &token, sizeof(token));
        }
    }

out:
    /* The usual cleanup. */
    if (fd >= 0)
        close(fd);
    umount(target);
    rmdir(target);

    return ret;
}

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
        if (!auth(&module, &module.device[i])) {
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

