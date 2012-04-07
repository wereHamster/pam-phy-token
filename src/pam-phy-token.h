
#ifndef __PAM_PHY_TOKEN__
#define __PAM_PHY_TOKEN__

#include <blkid.h>
#include <uuid/uuid.h>

/* Pretty-print an UUID. */
const char *__uuid(const unsigned char *uuid);


/* The token is an 8 byte unix timestamp followed by (4096-8) random bytes. */
struct token {
    uint64_t time;
    unsigned char data[4096 - 8];
} __attribute__((packed));

/* This is a helper function which generates one such token. */
void mktoken(struct token *token);


/* Each device which is used to store the token is identified by an UUID, and
 * we keep a copy of the token on the host so we can compare the validity of
 * the device. */
struct pam_phy_device {
    uuid_t uuid;
    struct token token;
} __attribute__((packed));


/* Each user can have several different tokens. When the PAM module is
 * initialized, it loads all the devices for the given user into memory. */
struct pam_phy_module {
    char user[64], node[64];
    blkid_cache cache;

    int count;
    struct pam_phy_device *device;
};


/* Functions used in both the PAM module and the helper to initialize and
 * cleanup the internal data structures. */
int pam_phy_module_init(struct pam_phy_module *module, const char *user);
int pam_phy_auth(struct pam_phy_module *module, struct pam_phy_device *device);
void pam_phy_module_cleanup(struct pam_phy_module *module);


/* Mount the filesystem identified by the given UUID. The mount target is
 * randomly generated, be sure to delete the directory after you unmount the
 * filesystem. */
const char *uuidmount(struct pam_phy_module *module, const char *uuid);

#endif /* __PAM_PHY_TOKEN__ */
