
#ifndef __PAM_PHY_TOKEN__
#define __PAM_PHY_TOKEN__

#include <blkid.h>
#include <uuid/uuid.h>

/* Pretty-print an UUID. */
const char *__uuid(const unsigned char *uuid);


/* The token is 4096 random bytes. */
struct token {
    uint64_t time;
    unsigned char data[4096 - 8];
} __attribute__((packed));

void mktoken(struct token *token);


struct pam_phy_device {
    uuid_t uuid;
    struct token token;
} __attribute__((packed));


struct pam_phy_module {
    char user[64], node[64];
    blkid_cache cache;

    int count;
    struct pam_phy_device *device;
};


int pam_phy_module_init(struct pam_phy_module *module, const char *user);
void pam_phy_module_cleanup(struct pam_phy_module *module);
const char *uuidmount(struct pam_phy_module *module, const char *uuid);

#endif /* __PAM_PHY_TOKEN__ */

