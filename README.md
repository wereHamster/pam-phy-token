
pam-phy-token
=============

pam-phy-token is a PAM module which checks the presence of a physical token
to allow or deny authentication. The physical token is any volume which can be
mounted and accessed through the filesystem syscalls, such as an USB stick.


Installation
------------

First obtain the source code.

    git clone git://github.com/wereHamster/pam-phy-token.git
    cd pam-phy-token

Build and install the pam module and a helper binary

    make && sudo make install


Setting up a physical token
---------------------------

Now you need to set up a volume you'd like to use as the physical token. The
helper `pam-phy-token` can be used to list all volumes currently attached to
the system.

    pam-phy-token list

Pick one volume which you want to use to store the token. Initialize the
volume, this will create a small file in a hidden directory. This file is used
to verify the authenticity of the physical token.

    pam-phy-token sync <uuid>


Configuring PAM
---------------

Now you need to configure PAM to use the pam-phy-token module. You can
configure PAM to require *either* a physical token or a password, or to
require *both*. The later is essentially a two-factor authentication.

The other question you have to ask yourself is in which services to enable the
physical token. You can enable it for all services, or only for local logins,
or even only for certain applications.

To fully explain how PAM works is outside of the scope of this document. I'll
just provide some examples with the most useful configurations.


Examples
--------

Require *either* a physical token or a password:

    auth    sufficient    pam_phy_token.so
    auth    required      pam_unix.so

Require *both* and fail immediately if the physical token is not present:

    auth    requisite     pam_phy_token.so
    auth    required      pam_unix.so

