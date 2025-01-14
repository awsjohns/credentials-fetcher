/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* clients/kinit/kinit.c - Initialize a credential cache */
/*
 * Copyright 1990, 2008 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include "autoconf.h"
#include "k5-int.h"
#include "k5-platform.h"        /* For asprintf and getopt */
#include <krb5.h>
#include "extern.h"
#include <locale.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <com_err.h>

#ifndef _WIN32
#define GET_PROGNAME(x) (strrchr((x), '/') ? strrchr((x), '/') + 1 : (x))
#else
#define GET_PROGNAME(x) max(max(strrchr((x), '/'), strrchr((x), '\\')) + 1,(x))
#endif

#ifdef HAVE_PWD_H
#include <pwd.h>
static char *
get_name_from_os()
{
    struct passwd *pw;

    pw = getpwuid(getuid());
    return (pw != NULL) ? pw->pw_name : NULL;
}
#else /* HAVE_PWD_H */
#ifdef _WIN32
static char *
get_name_from_os()
{
    static char name[1024];
    DWORD name_size = sizeof(name);

    if (GetUserName(name, &name_size)) {
        name[sizeof(name) - 1] = '\0'; /* Just to be extra safe */
        return name;
    } else {
        return NULL;
    }
}
#else /* _WIN32 */
static char *
get_name_from_os()
{
    return NULL;
}
#endif /* _WIN32 */
#endif /* HAVE_PWD_H */

static char *progname;

typedef enum { INIT_PW, INIT_KT, RENEW, VALIDATE } action_type;

struct k_opts
{
    /* In seconds */
    krb5_deltat starttime;
    krb5_deltat lifetime;
    krb5_deltat rlife;

    int forwardable;
    int proxiable;
    int request_pac;
    int anonymous;
    int addresses;

    int not_forwardable;
    int not_proxiable;
    int not_request_pac;
    int no_addresses;

    int verbose;

    char *principal_name;
    char *service_name;
    char *keytab_name;
    char *k5_in_cache_name;
    char *k5_out_cache_name;
    char *armor_ccache;

    action_type action;
    int use_client_keytab;

    int num_pa_opts;
    krb5_gic_opt_pa_data *pa_opts;

    int canonicalize;
    int enterprise;
};

struct k5_data
{
    krb5_context ctx;
    krb5_ccache in_cc, out_cc;
    krb5_principal me;
    char *name;
    krb5_boolean switch_to_cache;
};

/*
 * If struct[2] == NULL, then long_getopt acts as if the short flag struct[3]
 * were specified.  If struct[2] != NULL, then struct[3] is stored in
 * *(struct[2]), the array index which was specified is stored in *index, and
 * long_getopt() returns 0.
 */
const char *shopts = "r:fpFPn54aAVl:s:c:kit:T:RS:vX:CEI:";

#define USAGE_BREAK "\n\t"

struct userdata {
	char name[256];
	char pass[256];
};
struct userdata udata;

static krb5_context errctx;
static void
extended_com_err_fn(const char *myprog, errcode_t code, const char *fmt,
                    va_list args)
{
    const char *emsg;

    emsg = krb5_get_error_message(errctx, code);
    fprintf(stderr, "%s: %s ", myprog, emsg);
    krb5_free_error_message(errctx, emsg);
    fprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
}

static int
k5_begin(struct k_opts *opts, struct k5_data *k5)
{
    krb5_error_code ret;
    int success = 0;
    int flags = opts->enterprise ? KRB5_PRINCIPAL_PARSE_ENTERPRISE : 0;
    krb5_ccache defcache = NULL;
    krb5_principal defcache_princ = NULL, princ;
    krb5_keytab keytab;
    const char *deftype = NULL;
    char *defrealm, *name;

    ret = krb5_init_context(&k5->ctx);
    if (ret) {
        com_err(progname, ret, _("while initializing Kerberos 5 library"));
        return 0;
    }
    errctx = k5->ctx;

    if (opts->k5_out_cache_name) {
        ret = krb5_cc_resolve(k5->ctx, opts->k5_out_cache_name, &k5->out_cc);
        if (ret) {
            com_err(progname, ret, _("resolving ccache %s"),
                    opts->k5_out_cache_name);
            goto cleanup;
        }
        if (opts->verbose) {
            fprintf(stderr, _("Using specified cache: %s\n"),
                    opts->k5_out_cache_name);
        }
    } else {
        /* Resolve the default ccache and get its type and default principal
         * (if it is initialized). */
        ret = krb5_cc_default(k5->ctx, &defcache);
        if (ret) {
            com_err(progname, ret, _("while getting default ccache"));
            goto cleanup;
        }
        deftype = krb5_cc_get_type(k5->ctx, defcache);
        if (krb5_cc_get_principal(k5->ctx, defcache, &defcache_princ) != 0)
            defcache_princ = NULL;
    }

    /* Choose a client principal name. */
    if (opts->principal_name != NULL) {
        /* Use the specified principal name. */
        ret = krb5_parse_name_flags(k5->ctx, opts->principal_name, flags,
                                    &k5->me);
        if (ret) {
            com_err(progname, ret, _("when parsing name %s"),
                    opts->principal_name);
            goto cleanup;
        }
    } else if (opts->anonymous) {
        /* Use the anonymous principal for the local realm. */
        ret = krb5_get_default_realm(k5->ctx, &defrealm);
        if (ret) {
            com_err(progname, ret, _("while getting default realm"));
            goto cleanup;
        }
        ret = krb5_build_principal_ext(k5->ctx, &k5->me,
                                       strlen(defrealm), defrealm,
                                       strlen(KRB5_WELLKNOWN_NAMESTR),
                                       KRB5_WELLKNOWN_NAMESTR,
                                       strlen(KRB5_ANONYMOUS_PRINCSTR),
                                       KRB5_ANONYMOUS_PRINCSTR, 0);
        krb5_free_default_realm(k5->ctx, defrealm);
        if (ret) {
            com_err(progname, ret, _("while building principal"));
            goto cleanup;
        }
    } else if (opts->action == INIT_KT && opts->use_client_keytab) {
        /* Use the first entry from the client keytab. */
        ret = krb5_kt_client_default(k5->ctx, &keytab);
        if (ret) {
            com_err(progname, ret,
                    _("When resolving the default client keytab"));
            goto cleanup;
        }
        ret = k5_kt_get_principal(k5->ctx, keytab, &k5->me);
        krb5_kt_close(k5->ctx, keytab);
        if (ret) {
            com_err(progname, ret,
                    _("When determining client principal name from keytab"));
            goto cleanup;
        }
    } else if (opts->action == INIT_KT) {
        /* Use the default host/service name. */
        ret = krb5_sname_to_principal(k5->ctx, NULL, NULL, KRB5_NT_SRV_HST,
                                      &k5->me);
        if (ret) {
            com_err(progname, ret,
                    _("when creating default server principal name"));
            goto cleanup;
        }
    } else if (k5->out_cc != NULL) {
        /* If the output ccache is initialized, use its principal. */
        if (krb5_cc_get_principal(k5->ctx, k5->out_cc, &princ) == 0)
            k5->me = princ;
    } else if (defcache_princ != NULL) {
        /* Use the default cache's principal, and use the default cache as the
         * output cache. */
        k5->out_cc = defcache;
        defcache = NULL;
        k5->me = defcache_princ;
        defcache_princ = NULL;
    }

    /* If we still haven't chosen, use the local username. */
    if (k5->me == NULL) {
        name = get_name_from_os();
        if (name == NULL) {
            fprintf(stderr, _("Unable to identify user\n"));
            goto cleanup;
        }
        ret = krb5_parse_name_flags(k5->ctx, name, flags, &k5->me);
        if (ret) {
            com_err(progname, ret, _("when parsing name %s"), name);
            goto cleanup;
        }
    }

    if (k5->out_cc == NULL && krb5_cc_support_switch(k5->ctx, deftype)) {
        /* Use an existing cache for the client principal if we can. */
        ret = krb5_cc_cache_match(k5->ctx, k5->me, &k5->out_cc);
        if (ret && ret != KRB5_CC_NOTFOUND) {
            com_err(progname, ret, _("while searching for ccache for %s"),
                    opts->principal_name);
            goto cleanup;
        }
        if (!ret) {
            if (opts->verbose) {
                fprintf(stderr, _("Using existing cache: %s\n"),
                        krb5_cc_get_name(k5->ctx, k5->out_cc));
            }
            k5->switch_to_cache = 1;
        } else if (defcache_princ != NULL) {
            /* Create a new cache to avoid overwriting the initialized default
             * cache. */
            ret = krb5_cc_new_unique(k5->ctx, deftype, NULL, &k5->out_cc);
            if (ret) {
                com_err(progname, ret, _("while generating new ccache"));
                goto cleanup;
            }
            if (opts->verbose) {
                fprintf(stderr, _("Using new cache: %s\n"),
                        krb5_cc_get_name(k5->ctx, k5->out_cc));
            }
            k5->switch_to_cache = 1;
        }
    }

    /* Use the default cache if we haven't picked one yet. */
    if (k5->out_cc == NULL) {
        k5->out_cc = defcache;
        defcache = NULL;
        if (opts->verbose) {
            fprintf(stderr, _("Using default cache: %s\n"),
                    krb5_cc_get_name(k5->ctx, k5->out_cc));
        }
    }

    if (opts->k5_in_cache_name) {
        ret = krb5_cc_resolve(k5->ctx, opts->k5_in_cache_name, &k5->in_cc);
        if (ret) {
            com_err(progname, ret, _("resolving ccache %s"),
                    opts->k5_in_cache_name);
            goto cleanup;
        }
        if (opts->verbose) {
            fprintf(stderr, _("Using specified input cache: %s\n"),
                    opts->k5_in_cache_name);
        }
    }

    ret = krb5_unparse_name(k5->ctx, k5->me, &k5->name);
    if (ret) {
        com_err(progname, ret, _("when unparsing name"));
        goto cleanup;
    }
    if (opts->verbose)
        fprintf(stderr, _("Using principal: %s\n"), k5->name);

    opts->principal_name = k5->name;

    success = 1;

cleanup:
    if (defcache != NULL)
        krb5_cc_close(k5->ctx, defcache);
    krb5_free_principal(k5->ctx, defcache_princ);
    return success;
}

static void
k5_end(struct k5_data *k5)
{
    krb5_free_unparsed_name(k5->ctx, k5->name);
    krb5_free_principal(k5->ctx, k5->me);
    if (k5->in_cc != NULL)
        krb5_cc_close(k5->ctx, k5->in_cc);
    if (k5->out_cc != NULL)
        krb5_cc_close(k5->ctx, k5->out_cc);
    krb5_free_context(k5->ctx);
    errctx = NULL;
    memset(k5, 0, sizeof(*k5));
}

static krb5_error_code KRB5_CALLCONV
kinit_prompter(krb5_context ctx, void *data, const char *name,
               const char *banner, int num_prompts, krb5_prompt prompts[])
{
    krb5_boolean *pwprompt = data;
    krb5_prompt_type *ptypes;
    krb5_error_code ret = 0;
    int i;

    /* Make a note if we receive a password prompt. */
    ptypes = krb5_get_prompt_types(ctx);
    for (i = 0; i < num_prompts; i++) {
        if (ptypes != NULL && ptypes[i] == KRB5_PROMPT_TYPE_PASSWORD)
            *pwprompt = TRUE;
    }

    printf("prompt at 0x%x, 0x%x, %s\n", prompts->reply->magic, prompts->reply->length, prompts->reply->data);
    //ret =  krb5_prompter_posix(ctx, data, name, banner, num_prompts, prompts);
    //printf("ret = %d\n", ret);
    printf("prompt at 0x%x, 0x%x, %s\n", prompts->reply->magic, prompts->reply->length, prompts->reply->data);
    prompts->reply->length = strlen(udata.pass);
    memcpy(prompts->reply->data, udata.pass, strlen(udata.pass));

    return ret;
}

static int
k5_kinit(struct k_opts *opts, struct k5_data *k5)
{
    int notix = 1;
    krb5_keytab keytab = 0;
    krb5_creds my_creds;
    krb5_error_code ret;
    krb5_get_init_creds_opt *options = NULL;
    krb5_boolean pwprompt = FALSE;
    krb5_address **addresses = NULL;
    krb5_principal cprinc;
    krb5_ccache mcc = NULL;
    int i;

    memset(&my_creds, 0, sizeof(my_creds));

    ret = krb5_get_init_creds_opt_alloc(k5->ctx, &options);
    if (ret)
        goto cleanup;

    if (opts->lifetime)
        krb5_get_init_creds_opt_set_tkt_life(options, opts->lifetime);
    if (opts->rlife)
        krb5_get_init_creds_opt_set_renew_life(options, opts->rlife);
    if (opts->forwardable)
        krb5_get_init_creds_opt_set_forwardable(options, 1);
    if (opts->not_forwardable)
        krb5_get_init_creds_opt_set_forwardable(options, 0);
    if (opts->proxiable)
        krb5_get_init_creds_opt_set_proxiable(options, 1);
    if (opts->not_proxiable)
        krb5_get_init_creds_opt_set_proxiable(options, 0);
    if (opts->canonicalize)
        krb5_get_init_creds_opt_set_canonicalize(options, 1);
    if (opts->anonymous)
        krb5_get_init_creds_opt_set_anonymous(options, 1);
    if (opts->addresses) {
        ret = krb5_os_localaddr(k5->ctx, &addresses);
        if (ret) {
            com_err(progname, ret, _("getting local addresses"));
            goto cleanup;
        }
        krb5_get_init_creds_opt_set_address_list(options, addresses);
    }
    if (opts->no_addresses)
        krb5_get_init_creds_opt_set_address_list(options, NULL);
    if (opts->armor_ccache != NULL) {
        krb5_get_init_creds_opt_set_fast_ccache_name(k5->ctx, options,
                                                     opts->armor_ccache);
    }
    if (opts->request_pac)
        krb5_get_init_creds_opt_set_pac_request(k5->ctx, options, TRUE);
    if (opts->not_request_pac)
        krb5_get_init_creds_opt_set_pac_request(k5->ctx, options, FALSE);


    if (opts->action == INIT_KT && opts->keytab_name != NULL) {
#ifndef _WIN32
        if (strncmp(opts->keytab_name, "KDB:", 4) == 0) {
            ret = kinit_kdb_init(&k5->ctx, k5->me->realm.data);
            errctx = k5->ctx;
            if (ret) {
                com_err(progname, ret,
                        _("while setting up KDB keytab for realm %s"),
                        k5->me->realm.data);
                goto cleanup;
            }
        }
#endif

        ret = krb5_kt_resolve(k5->ctx, opts->keytab_name, &keytab);
        if (ret) {
            com_err(progname, ret, _("resolving keytab %s"),
                    opts->keytab_name);
            goto cleanup;
        }
        if (opts->verbose)
            fprintf(stderr, _("Using keytab: %s\n"), opts->keytab_name);
    } else if (opts->action == INIT_KT && opts->use_client_keytab) {
        ret = krb5_kt_client_default(k5->ctx, &keytab);
        if (ret) {
            com_err(progname, ret, _("resolving default client keytab"));
            goto cleanup;
        }
    }

    for (i = 0; i < opts->num_pa_opts; i++) {
        ret = krb5_get_init_creds_opt_set_pa(k5->ctx, options,
                                             opts->pa_opts[i].attr,
                                             opts->pa_opts[i].value);
        if (ret) {
            com_err(progname, ret, _("while setting '%s'='%s'"),
                    opts->pa_opts[i].attr, opts->pa_opts[i].value);
            goto cleanup;
        }
        if (opts->verbose) {
            fprintf(stderr, _("PA Option %s = %s\n"), opts->pa_opts[i].attr,
                    opts->pa_opts[i].value);
        }
    }
    if (k5->in_cc) {
        ret = krb5_get_init_creds_opt_set_in_ccache(k5->ctx, options,
                                                    k5->in_cc);
        if (ret)
            goto cleanup;
    }
    ret = krb5_get_init_creds_opt_set_out_ccache(k5->ctx, options, k5->out_cc);
    if (ret)
        goto cleanup;

    switch (opts->action) {
    case INIT_PW:
        ret = krb5_get_init_creds_password(k5->ctx, &my_creds, k5->me, 0,
                                           kinit_prompter, &pwprompt,
                                           opts->starttime, opts->service_name,
                                           options);
        break;
    case INIT_KT:
        ret = krb5_get_init_creds_keytab(k5->ctx, &my_creds, k5->me, keytab,
                                         opts->starttime, opts->service_name,
                                         options);
        break;
    case VALIDATE:
        ret = krb5_get_validated_creds(k5->ctx, &my_creds, k5->me, k5->out_cc,
                                       opts->service_name);
        break;
    case RENEW:
        ret = krb5_get_renewed_creds(k5->ctx, &my_creds, k5->me, k5->out_cc,
                                     opts->service_name);
        break;
    }

    if (ret) {
        char *doing = NULL;
        switch (opts->action) {
        case INIT_PW:
        case INIT_KT:
            doing = _("getting initial credentials");
            break;
        case VALIDATE:
            doing = _("validating credentials");
            break;
        case RENEW:
            doing = _("renewing credentials");
            break;
        }

        /* If reply decryption failed, or if pre-authentication failed and we
         * were prompted for a password, assume the password was wrong. */
        if (ret == KRB5KRB_AP_ERR_BAD_INTEGRITY ||
            (pwprompt && ret == KRB5KDC_ERR_PREAUTH_FAILED)) {
            fprintf(stderr, _("%s: Password incorrect while %s\n"), progname,
                    doing);
        } else {
            com_err(progname, ret, _("while %s"), doing);
        }
        goto cleanup;
    }

    if (opts->action != INIT_PW && opts->action != INIT_KT) {
        cprinc = opts->canonicalize ? my_creds.client : k5->me;
        ret = krb5_cc_new_unique(k5->ctx, "MEMORY", NULL, &mcc);
        if (!ret)
            ret = krb5_cc_initialize(k5->ctx, mcc, cprinc);
        if (ret) {
            com_err(progname, ret, _("when creating temporary cache"));
            goto cleanup;
        }
        if (opts->verbose)
            fprintf(stderr, _("Initialized cache\n"));

        ret = k5_cc_store_primary_cred(k5->ctx, mcc, &my_creds);
        if (ret) {
            com_err(progname, ret, _("while storing credentials"));
            goto cleanup;
        }
        ret = krb5_cc_move(k5->ctx, mcc, k5->out_cc);
        if (ret) {
            com_err(progname, ret, _("while saving to cache %s"),
                    opts->k5_out_cache_name ? opts->k5_out_cache_name : "");
            goto cleanup;
        }
        mcc = NULL;
        if (opts->verbose)
            fprintf(stderr, _("Stored credentials\n"));
    }
    notix = 0;
    if (k5->switch_to_cache) {
        ret = krb5_cc_switch(k5->ctx, k5->out_cc);
        if (ret) {
            com_err(progname, ret, _("while switching to new ccache"));
            goto cleanup;
        }
    }

cleanup:
#ifndef _WIN32
    kinit_kdb_fini();
#endif
    if (mcc != NULL)
        krb5_cc_destroy(k5->ctx, mcc);
    if (options)
        krb5_get_init_creds_opt_free(k5->ctx, options);
    if (my_creds.client == k5->me)
        my_creds.client = 0;
    if (opts->pa_opts) {
        free(opts->pa_opts);
        opts->pa_opts = NULL;
        opts->num_pa_opts = 0;
    }
    krb5_free_cred_contents(k5->ctx, &my_creds);
    if (keytab != NULL)
        krb5_kt_close(k5->ctx, keytab);
    return notix ? 0 : 1;
}

int
my_kinit_main(int argc, char *argv[])
{
    struct k_opts opts;
    struct k5_data k5;
    int authed_k5 = 0;

    strncpy(udata.name, argv[1], strlen(argv[1]) + 1);
    strncpy(udata.pass, argv[2], strlen(argv[2]) + 1);

    setlocale(LC_ALL, "");
    progname = GET_PROGNAME(argv[0]);

    /* Ensure we can be driven from a pipe */
    if (!isatty(fileno(stdin)))
        setvbuf(stdin, 0, _IONBF, 0);
    if (!isatty(fileno(stdout)))
        setvbuf(stdout, 0, _IONBF, 0);
    if (!isatty(fileno(stderr)))
        setvbuf(stderr, 0, _IONBF, 0);

    memset(&opts, 0, sizeof(opts));
    opts.action = INIT_PW;
    opts.principal_name = argv[1];
    opts.verbose = 1;

    memset(&k5, 0, sizeof(k5));

    set_com_err_hook(extended_com_err_fn);

    if (k5_begin(&opts, &k5))
        authed_k5 = k5_kinit(&opts, &k5);

    if (authed_k5 && opts.verbose)
        fprintf(stderr, _("Authenticated to Kerberos v5\n"));

    k5_end(&k5);

    memset(udata.name, 0xff, sizeof(udata.name));
    memset(udata.pass, 0xff, sizeof(udata.pass));

    if (!authed_k5)
        return(1);
    return 0;
}
