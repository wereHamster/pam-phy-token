
#include "pam-phy-token.h"

#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/mount.h>
#include <string.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

static struct pam_phy_device *find(struct pam_phy_module *module, const char *uuid)
{
    if (module->count == 0)
        return NULL;

    uuid_t _uuid;
    uuid_parse(uuid, _uuid);

    for (int i = 0; i < module->count; ++i) {
        if (uuid_compare(_uuid, module->device[i].uuid) == 0) {
            return &module->device[i];
        }
    }

    return NULL;
}

static void list(struct pam_phy_module *module, int argc, char *argv[])
{
    blkid_dev dev;
    blkid_dev_iterate iter = blkid_dev_iterate_begin(module->cache);

    while (!blkid_dev_next(iter, &dev)) {
        const char *devname = blkid_dev_devname(dev);
        const char *uuid = blkid_get_tag_value(module->cache, "UUID", devname);
        const char *type = blkid_get_tag_value(module->cache, "TYPE", devname);

        printf("device %s\n", uuid);
        printf("   devname: %s, filesystem: %s\n", devname, type);

        struct pam_phy_device *device = find(module, uuid);
        if (device) {
            time_t time = device->token.time;
            struct tm *tm = localtime(&time);
            char timebuf[100];
            strftime(timebuf, 100, "%F %T ", tm);
            printf("   last used: %s\n", timebuf);
        }
        printf("\n");
    }

    blkid_dev_iterate_end(iter);
}

static void sync(struct pam_phy_module *module, int argc, char *argv[])
{
    if (argc < 1)
        return;

    printf("Syncing with device %s\n", argv[0]);
    const char *target = uuidmount(module, argv[0]);
    printf("Mounted to %s\n", target);

    char path[4096];
    snprintf(path, 4096, "%s/.pam-phy-token", target);
    mkdir(path, 0700);

    struct token token;
    mktoken(&token);

    snprintf(path, 4096, "%s/.pam-phy-token/%s@%s", target, module->user, module->node);
    int fd = open(path, O_CREAT|O_WRONLY|O_TRUNC, 0600);
    write(fd, &token, sizeof(token));
    close(fd);

    printf("Path: %s, %s\n", path, getenv("HOME"));

    snprintf(path, 4096, "/etc/pam-phy-token/%s", module->user);
    printf("Path: %s\n", path);

    struct pam_phy_device device;
    uuid_parse(argv[0], device.uuid);
    memcpy(&device.token, &token, sizeof(token));

    fd = open(path, O_CREAT|O_WRONLY|O_TRUNC, 0600);
    write(fd, &device, sizeof(device));
    close(fd);

    umount(target);
    rmdir(target);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
        return 0;

    struct passwd *pwd = getpwuid(getuid());

    struct pam_phy_module module;
    if (pam_phy_module_init(&module, pwd->pw_name)) {
        printf("Failed to initialize pam-phy-token\n");
        return 1;
    }

    if (strcmp(argv[1], "list") == 0) {
        list(&module, argc - 2, &argv[2]);
    } else if (strcmp(argv[1], "sync") == 0) {
        sync(&module, argc - 2, &argv[2]); 
    } else {
        printf("%s [list|sync]\n", argv[0]);
    }

    pam_phy_module_cleanup(&module);

    return 0;
}
