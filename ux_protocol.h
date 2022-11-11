#ifndef UX_PROTOCOL_H
#define UX_PROTOCOL_H

//#include "ux_protocol.h"

enum enum_transmit
{
    e_register,
    e_register_back,

    e_login,
    e_login_back,

    e_logout,
    e_error,
    e_transmit
};

struct ct_cmd
{
    int type;
};

struct ct_register
{
    char passwd[64];
};

struct ct_register_back
{
    long long account;
    char passwd[64];
};

struct ct_login
{
    long long account;
    char passwd[64];
};

struct ct_login_back
{
    int flg;
    char info[64];
};

struct ct_logout
{
    long long account;
};

struct ct_error
{
    long long account;
    char error[64];
};

struct ct_transmit
{
    long long account_from;
    long long account_to;
    char data[1024];
};


#endif // UX_PROTOCOL_H
