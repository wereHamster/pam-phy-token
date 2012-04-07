
#define _XOPEN_SOURCE 500
#define _XOPEN_SOURCE_EXTENDED

#include "pam-phy-token.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

const char *__uuid(const uuid_t uuid)
{
    static char buffer[100];
    uuid_unparse(uuid, buffer);
    return buffer;
}

/* Generate a new token. Use /dev/urandom, and if that fails fall back to
 * random() in a loop. */
void mktoken(struct token *token)
{
    /* Set the time to the current time. */
    token->time = time(NULL);

    /* Then fill the remaining space with random bytes. */
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        read(fd, token->data, sizeof(token->data));
        close(fd);
    } else {
        srandom(time(NULL));
        for (int i = 0; i < sizeof(token->data); ++i) {
            token->data[i] = random() & 0xff;
        }
    }
}

/* Initialize the pam-phy-token module. */
int pam_phy_module_init(struct pam_phy_module *module, const char *user)
{
    /* Copy the user and hostname to the module structure, so we don't have to
     * pass it around all the time. */
    strncpy(module->user, user, sizeof(module->user));
    gethostname(module->node, sizeof(module->node));

    /* Initialize the blkid cache, and probe all devices. */
    if (blkid_get_cache(&module->cache, NULL))
        return -1;
    blkid_probe_all(module->cache);

    /* The path to the user's token store. */
    char path[100];
    snprintf(path, sizeof(path), "/etc/pam-phy-token/%s", user);

    /* Try to open that file, if it succeeds, map its contents. */
    int fd = open(path, O_RDWR);
    if (fd >= 0) {
        struct stat stbuf;
        if (!fstat(fd, &stbuf)) {
            module->count = stbuf.st_size / sizeof(struct pam_phy_device);
            int size = module->count * sizeof(struct pam_phy_device);

            /* Map the file into memory. */
            module->device = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (module->device == MAP_FAILED)
                module->count = 0;
        }

        close(fd);
    } else {
        /* If the file dosn't exist, set the count to zero. */
        module->count = 0;
    }

    return 0;
}

int pam_phy_auth(struct pam_phy_module *module, struct pam_phy_device *device)
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

/* Cleanup all that stuff. */
void pam_phy_module_cleanup(struct pam_phy_module *module)
{
    if (module->count)
        munmap(module->device, module->count * sizeof(struct pam_phy_device));

    blkid_put_cache(module->cache);
}

/* Fill the buffer with the path to a random mount target. */
static void random_mount_target(char *buffer)
{
    strcpy(buffer, "/mnt/");

    uuid_t random;
    uuid_generate_random(random);
    uuid_unparse(random, buffer + 5);
}

/* Mount a volume by its UUID. Return the path where the filesystem is mounted
 * to. You have to free that string after you umount the volume! */
const char *uuidmount(struct pam_phy_module *module, const char *uuid)
{
    /* Locate the devname by the UUID. */
    const char *dev = blkid_evaluate_tag("UUID", uuid, &module->cache);
    if (!dev)
        return NULL;

    /* Generate the target directory. We use an UUID, but not the same as the
     * volume. This is to avoid races when a user logs in at the same time on
     * two different consoles. */
    char target[1024];
    random_mount_target(target);

    /* Attempt to create that directory. It should not exist. */
    if (mkdir(target, 0700))
        return NULL;

    /* Get the filesystem type. */
    const char *type = blkid_get_tag_value(module->cache, "TYPE", dev);

    /* Now attempt to mount the filesystem. We try to disable as many features
     * as possible through the flags. */
    int flags = MS_NOATIME | MS_NODEV | MS_NOEXEC | MS_NOSUID;
    if (mount(dev, target, type, flags, NULL))
        return NULL;

    /* Return a copy of the target dire. Make sure to free it after you
     * unmount the filesystem. */
    return strdup(target);
}
