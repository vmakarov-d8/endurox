/* 
** Cache event receiver, but what we do not want is to subscribe on local events
** if subscribed by mask, but if it is local node, then just ignore
** Also we advertise service name with node id in it. So that remote node in future
** may call service directly.
** -----
** Also server provides management services (like cache lookup and delete)
**
** @file tpcachesv.c
** 
** -----------------------------------------------------------------------------
** Enduro/X Middleware Platform for Distributed Transaction Processing
** Copyright (C) 2018 Mavimax, Ltd. All Rights Reserved.
** This software is released under one of the following licenses:
** GPL or Mavimax's license for commercial use.
** -----------------------------------------------------------------------------
** GPL license:
** 
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; either version 2 of the License, or (at your option) any later
** version.
**
** This program is distributed in the hope that it will be useful, but WITHOUT ANY
** WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
** PARTICULAR PURPOSE. See the GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License along with
** this program; if not, write to the Free Software Foundation, Inc., 59 Temple
** Place, Suite 330, Boston, MA 02111-1307 USA
**
** -----------------------------------------------------------------------------
** A commercial use license is available from Mavimax, Ltd
** contact@mavimax.com
** -----------------------------------------------------------------------------
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <regex.h>
#include <utlist.h>

#include <ndebug.h>
#include <atmi.h>
#include <sys_unix.h>
#include <atmi_int.h>
#include <typed_buf.h>
#include <ndrstandard.h>
#include <ubf.h>
#include <Exfields.h>
#include <atmi_shm.h>
#include <exregex.h>
#include "tpcachesv.h"
#include <atmi_cache.h>
/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/
/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/

/**
 * Process incoming events - put and delete
 * @param p_svc
 */
void CACHEEV (TPSVCINFO *p_svc)
{
    int ret=EXSUCCEED;
    tp_command_call_t * last_call;
    char extradata[XATMI_EVENT_MAX+1];
    char nodeidstr[3+1];
    char *op;
    int nodeid;
    char *flags;
    char *svcnm;
    int len;
    
    /* now understand what request this was 
     * also we need to get timestamps
     */
    last_call=ndrx_get_G_last_call();
    
    NDRX_STRCPY_SAFE(extradata, last_call->extradata);
    
    NDRX_LOG(log_info, "Received event op: [%s]", extradata);
    
    if (NULL==(op = strtok(extradata, "/")))
    {
        NDRX_LOG(log_error, "Invalid event [%s] received - failed to get 'operation'", 
                last_call->extradata);
        EXFAIL_OUT(ret);
    }
    
    len = strlen(op);
    
    if (len != NDRX_CACHE_EV_PFXLEN)
    {
        NDRX_LOG(log_error, "Invalid event prefix, expected len: %d, got: %d",
                NDRX_CACHE_EV_PFXLEN, len);
        EXFAIL_OUT(ret);
    }
    
    nodeidstr[0] = op[3];
    nodeidstr[1] = op[3+1];
    nodeidstr[2] = op[3+2];
    nodeidstr[3] = EXEOS;
    
    nodeid = atoi(nodeidstr);
    
    if (nodeid<=0)
    {
        NDRX_LOG(log_error, "Invalid node id received [%d] must be > 0!", 
                nodeid);
        EXFAIL_OUT(ret);
    }
    
    op[3] = EXEOS;
    
    if (NULL==(flags = strtok(NULL, "/")))
    {
        NDRX_LOG(log_error, "Invalid event [%s] received - failed to get 'flags'",
                last_call->extradata);
        EXFAIL_OUT(ret);
    }
    
    if (NULL==(svcnm = strtok(NULL, "/")))
    {
        NDRX_LOG(log_error, "Invalid event [%s] received - failed to get "
                "'service name' for cache op", last_call->extradata);
        EXFAIL_OUT(ret);
    }
    
    NDRX_LOG(log_info, "Received operation [%s] flags [%s] for service [%s]",
            op, flags, svcnm);
    
    /* check is op correct? */
    if (0==strcmp(op, NDRX_CACHE_EV_PUTCMD))
    {
        NDRX_LOG(log_debug, "performing put (save to cache)...");

        /* ok we shall get the cluster node id of the caller.. */
        if (EXSUCCEED!=ndrx_cache_save (svcnm, p_svc->data, 
            p_svc->len, last_call->user3, last_call->user4, nodeid, 0L,
                /* user1 & user2: */
                last_call->rval, last_call->rcode, EXTRUE))
        {
            NDRX_LOG(log_error, "Failed to save cache data: %s", 
                    tpstrerror(tperrno));
            EXFAIL_OUT(ret);
        }
    }
    else if (0==strcmp(op, NDRX_CACHE_EV_DELCMD))
    {
        /* Delete cache according to flags, if FULL specified, then drop all matched 
         * cache (no matter of they keys)
         */
        if (EXSUCCEED!=ndrx_cache_inval_by_data(svcnm, p_svc->data, p_svc->len, flags))
        {
            NDRX_LOG(log_error, "Failed to save cache data: %s",
                    tpstrerror(tperrno));
            EXFAIL_OUT(ret);
        }
    }
    else if (0==strcmp(op, NDRX_CACHE_EV_KILCMD))
    {
        /*
         * In this case the svcnm is database and we remove all records from it
         */
        if (EXSUCCEED!=ndrx_cache_drop(svcnm))
        {
            NDRX_LOG(log_error, "Failed to drop cache: %s", tpstrerror(tperrno));
            EXFAIL_OUT(ret);
        }
    }
    else if (0==strcmp(op, NDRX_CACHE_EV_MSKDELCMD))
    {
        /* TODO: delete by key mask, received UBF buffer 
         * This is sent from xadmin tooling
         * EX_CACHE_OPEXPR -> Operation expression
         * EX_CACHE_DBNAME -> Db name of cache
         * EX_CACHE_BUFTYP -> buffer type id
         * EX_CACHE_DUMP   -> Data dump, carray
         * EX_CACHE_TPERRNO
         * EX_CACHE_TPRUCODE
         * EX_CACHE_TIM
         * EX_CACHE_TIMUSEC
         * EX_CACHE_HITT
         * EX_CACHE_HITTU
         * EX_CACHE_NODEID
         * EX_CACHE_CMDS  -> flags of commands
         */
        /* DB name should be in event, we just need OPEXPR to remove records by
         * regex
         */
        
    }
    else
    {
        NDRX_LOG(log_error, "Unsupported cache command received [%s]",
                op);
        EXFAIL_OUT(ret);
    }
    
out:
    tpreturn(  ret==EXSUCCEED?TPSUCCESS:TPFAIL,
                0,
                NULL,
                0L,
                0L);
}

/**
 * Process management requests (list headers, list all, list 
 * single, delete all, delete single).
 * The dump shall be provided in tpexport format.
 * 
 * @param p_svc
 */
void CACHEMG (TPSVCINFO *p_svc)
{
    int ret=EXSUCCEED;

out:
    tpreturn(  ret==EXSUCCEED?TPSUCCESS:TPFAIL,
                0,
                NULL,
                0L,
                0L);
}

/**
 * Standard server init
 */
int NDRX_INTEGRA(tpsvrinit)(int argc, char **argv)
{
    int ret=EXSUCCEED;
    char cachesvc[MAXTIDENT+1];
    char mgmtsvc[MAXTIDENT+1];
    long nodeid;
    string_list_t *list = NULL, *el;
    TPEVCTL evctl;
    
    NDRX_LOG(log_debug, "%s called", __func__);
    
    memset(&evctl, 0, sizeof(evctl));
    
    nodeid = tpgetnodeid();
    
    if (EXFAIL==nodeid)
    {
        NDRX_LOG(log_error, "Failed to get nodeid: %s", tpstrerror(tperrno));
        EXFAIL_OUT(ret);
    }
    
    snprintf(cachesvc, sizeof(cachesvc), NDRX_CACHE_EVSVC, nodeid);

    if (EXSUCCEED!=tpadvertise(cachesvc, CACHEEV))
    {
        NDRX_LOG(log_error, "Failed to initialize [%s]!", cachesvc);
        EXFAIL_OUT(ret);
    }
    
    snprintf(mgmtsvc, sizeof(mgmtsvc), NDRX_CACHE_MGSVC, nodeid);
    
    if (EXSUCCEED!=tpadvertise(mgmtsvc, CACHEEV))
    {
        NDRX_LOG(log_error, "Failed to initialize [%s]!", mgmtsvc);
        EXFAIL_OUT(ret);
    }
        
    /* Process the caches and subscribe to corresponding events */
    
    if (EXSUCCEED!=ndrx_cache_events_get(&list))
    {
        NDRX_LOG(log_error, "Failed to get list of events to subscribe to!");
        EXFAIL_OUT(ret);
    }
    
    /* loop over the list and subscribe to */
    
    LL_FOREACH(list, el)
    {
        NDRX_STRCPY_SAFE(evctl.name1, cachesvc);
        evctl.flags|=TPEVSERVICE;
        
        if (EXSUCCEED!=tpsubscribe(el->qname, NULL, &evctl, 0L))
        {
            NDRX_LOG(log_error, "Failed to subscribe to event: [%s] "
                "target service: [%s]", el->qname, evctl.name1);
            EXFAIL_OUT(ret);
        }
    }
    
out:

    if (NULL!=list)
    {
        ndrx_string_list_free(list);
    }

    return ret;
}

void NDRX_INTEGRA(tpsvrdone) (void)
{
    NDRX_LOG(log_debug, "%s called", __func__);
}
