/* 
** Memory checking library
**
** @file exmemcklib.c
** 
** -----------------------------------------------------------------------------
** Enduro/X Middleware Platform for Distributed Transaction Processing
** Copyright (C) 2015, Mavimax, Ltd. All Rights Reserved.
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
/*---------------------------Includes-----------------------------------*/
#include <stdlib.h>
#include <stdio.h>
#include <ndrstandard.h>
#include <nstdutil.h>
#include <nstd_tls.h>
#include <string.h>
#include "thlock.h"
#include "userlog.h"
#include "ndebug.h"
#include <exmemck.h>
#include <errno.h>
/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/

exmemck_config_t *M_config = NULL; /* global config */
exmemck_process_t *M_proc = NULL; /* global process list */

/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/

/**
 * Get config
 * @param mask
 * @param autocreate create entry automatically...
 * @return found/allocated config block or NULL
 */
exprivate exmemck_config_t * get_config(char *mask, int autocreate, int *p_ret, 
        int *p_is_new)
{
    exmemck_config_t * ret;
    
    EXHASH_FIND_STR(M_config, mask, ret);
    
    if (NULL==ret && autocreate)
    {
        /* Allocate the block */
        
        if (EXSUCCEED!=(ret=NDRX_CALLOC(1, sizeof(exmemck_config_t))))
        {
            int err = errno;
            NDRX_LOG(log_error, "Failed to allocate xmemck_config_t: %s", 
                    strerror(err));
            
            userlog("Failed to allocate xmemck_config_t: %s", 
                    strerror(err));
            EXFAIL_OUT((*p_ret));
        }
        
        NDRX_STRCPY_SAFE(ret->mask, mask);
        
        EXHASH_ADD_STR(M_config, mask, ret);
        
        *p_is_new = EXTRUE;
        
    }
out:

    return ret;
}
/**
 * Dump the configuration to log file
 * @param cfg configuration node
 */
exprivate void dump_config(exmemck_config_t * cfg)
{
    NDRX_LOG(log_debug, "=== Config entry, mask: [%s] ======", cfg->mask);
    NDRX_LOG(log_debug, "inheritted defaults from mask: [%s]", cfg->dlft_mask);
    NDRX_LOG(log_debug, "mem_limit                    : [%ld]", cfg->settings.mem_limit);
    NDRX_LOG(log_debug, "percent_diff_allow           : [%ld]", cfg->settings.percent_diff_allow);
    NDRX_LOG(log_debug, "interval_start_prcnt         : [%ld]", cfg->settings.interval_start_prcnt);
    NDRX_LOG(log_debug, "interval_stop_prcnt          : [%ld]", cfg->settings.interval_stop_prcnt);
    NDRX_LOG(log_debug, "flags                        : [0x%lx]", cfg->settings.flags);
    NDRX_LOG(log_debug, "interval_mon                 : [%d]", cfg->settings.interval_mon);
    NDRX_LOG(log_debug, "pf_proc_exit                 : [%p]", cfg->settings.pf_proc_exit);
    NDRX_LOG(log_debug, "pf_proc_leaky                : [%p]", cfg->settings.pf_proc_leaky);
    NDRX_LOG(log_debug, "===================================="); 
}

/**
 * Add configuration entry to the xm lib. Each entry will epply to the
 * list of the processes
 * @param mask    Mask key
 * @param dfld_mask Default mask
 * @param p_settings    Settings for the monitoring
 * @return SUCCEED/FAIL
 */
expublic int ndrx_memck_add(char *mask,  char *dlft_mask, exmemck_settings_t *p_settings )
{
    int ret = EXSUCCEED;
    int is_new = EXFALSE;
    
    exmemck_config_t * cfg, *dflt;
    
    NDRX_LOG(log_debug, "%s: enter, mask: [%s]", __func__, mask);
    
    cfg = get_config(mask, EXTRUE, &ret, &is_new);
    
    if (NULL==cfg || EXSUCCEED!=ret)
    {
        NDRX_LOG(log_error, "%s: failed to get config for mask [%s]", 
                __func__, mask);
        EXFAIL_OUT(ret);
    }
    
    /* search for defaults */
    if (is_new && NULL!=dlft_mask)
    {
        NDRX_LOG(log_debug, "Making init for defaults: [%s]", dlft_mask);
        
        /* ret will succeed here always! */
        dflt = get_config(dlft_mask, EXFALSE, &ret, NULL);
        
        if (NULL!=dflt)
        {
            /* Got defaults */
            
            NDRX_LOG(log_debug, "Got defaults...");
            memcpy(&(cfg->settings), &(dflt->settings), sizeof(cfg->settings));
            
            NDRX_STRCPY_SAFE(cfg->dlft_mask, dlft_mask);
        }
    }
    
    if (EXFAIL < p_settings->flags)
    {
        cfg->settings.flags = p_settings->flags;
    }
    
    if (EXFAIL < p_settings->interval_start_prcnt)
    {
        cfg->settings.interval_start_prcnt = p_settings->interval_start_prcnt;
    }
    
    if (EXFAIL < p_settings->interval_stop_prcnt)
    {
        cfg->settings.interval_stop_prcnt = p_settings->interval_stop_prcnt;
    }
    
    if (EXFAIL < p_settings->mem_limit)
    {
        cfg->settings.mem_limit = p_settings->mem_limit;
    }
    
    if (EXFAIL < p_settings->percent_diff_allow)
    {
        cfg->settings.percent_diff_allow = p_settings->percent_diff_allow;
    }
    
    if (NULL!=p_settings->pf_proc_exit)
    {
        cfg->settings.pf_proc_exit = p_settings->pf_proc_exit;
    }
    
    if (NULL!=p_settings->pf_proc_leaky)
    {
        cfg->settings.pf_proc_leaky = p_settings->pf_proc_leaky;
    }
    
    
    dump_config(cfg);
   
out:    
    NDRX_LOG(log_debug, "%s returns %d", __func__, ret);

    return ret;
}

/**
 * Remove process
 * @param el
 * @return 
 */
exprivate void rm_proc(exmemck_process_t *el)
{
    EXHASH_DEL(M_proc, el);
    
    NDRX_FREE(el->stats);
    
    NDRX_FREE(el);
}

/**
 * Remove process by mask. Also will remove any monitored processes from the list
 * If mask is not found, then function will return SUCCEED anyway...
 * @param mask Mask to search for and remote, this will kill all the processes
 * monitored by this mask
 * @return SUCCEED/FAIL
 */
expublic int ndrx_memck_rm(char *mask)
{
    int ret = EXSUCCEED;
    exmemck_config_t * cfg;
    
    exmemck_process_t *el = NULL;
    exmemck_process_t *elt = NULL;
    
    NDRX_LOG(log_debug, "%s enter, mask: [%s]", __func__, mask);
    
    cfg = get_config(mask, EXFALSE, NULL,  NULL);
    
    if (NULL!=cfg)
    {
        NDRX_LOG(log_debug, "Entry found - removing...");
        EXHASH_DEL(M_config, cfg);
        
        /* Loop over the processes and kill with this config */
        
        /* Iterate over the hash! */
        EXHASH_ITER(hh, M_proc, el, elt)
        {
            if (el->p_config == cfg)
            {
                NDRX_LOG(log_info, "deleting process: %d by mask [%s]", 
                        el->pid, mask);
                rm_proc(el);
            }
        }
        
        NDRX_FREE(cfg);
    }
out:
    return ret;
}

/**
 * Calculate statistics for single process
 * if finding out that it is leaking, call the callback if defined.
 * Set the status...
 */
exprivate void calc_stat(exmemck_process_t *proc)
{
    int ret = EXSUCCEED;
    int i;
    double start;
    int start_i;
    double stop;
    int stop_i;
    long rss;
    long vsz;
    
    long diff_rss;
    long diff_vsz;
    
    double rss_increase_prcnt;
    double vsz_increase_prcnt;
    
    int first_halve_start;
    int second_halve_start;
    int second_halve_finish;
    
    NDRX_LOG(log_debug, "%s: enter, pid=%d", __func__, proc->pid);
    
    /* read the statistics entry... */
    start = (double)proc->nr_of_stats * 
            ((double)proc->p_config->settings.interval_start_prcnt)/100.0f;
    stop = (double)proc->nr_of_stats * 
            ((double)proc->p_config->settings.interval_stop_prcnt)/100.0f;
    
    start_i = (int)start;
    stop_i = (int)stop;
    
    
    first_halve_start = start_i;
    second_halve_start = start_i + (stop_i - start_i)/2;
    second_halve_finish = stop_i;
    
    if (first_halve_start == second_halve_start)
    {
        NDRX_LOG(log_debug, "No stats available for pid=%d (start==stop)", proc->pid);
        goto out;
    }
    
    /* first halve */
    rss = 0;
    vsz = 0;
    
    NDRX_LOG(log_debug, "first halve loop [%d..%d]", first_halve_start, 
            second_halve_start-1);
    for (i=first_halve_start; i<second_halve_start; i++)
    {
        rss+=proc->stats[i].rss;
        vsz+=proc->stats[i].vsz;
    }
    
    proc->avg_first_halve_rss = rss / (second_halve_start - first_halve_start + 1);
    proc->avg_first_halve_vsz = vsz / (second_halve_start - first_halve_start + 1);
    
    /* second halve */
    rss = 0;
    vsz = 0;
    
    NDRX_LOG(log_debug, "second halve loop [%d..%d]", first_halve_start, 
            second_halve_start-1);
    for (i=second_halve_start; i<second_halve_finish; i++)
    {
        rss+=proc->stats[i].rss;
        vsz+=proc->stats[i].vsz;
    }
    
    proc->avg_second_halve_rss = rss / (second_halve_finish - second_halve_start + 1);
    proc->avg_second_halve_vsz = vsz / (second_halve_finish - second_halve_start + 1);
    
    diff_rss = proc->avg_second_halve_rss - proc->avg_first_halve_rss;
    diff_vsz = proc->avg_second_halve_vsz - proc->avg_second_halve_vsz;
    
    /* recalc the flags */
    
    proc->status&=(~EXMEMCK_STATUS_LEAKY_RSS);
    proc->status&=(~EXMEMCK_STATUS_LEAKY_VSZ);
    
    
    rss_increase_prcnt = ((double)diff_rss)/((double)proc->avg_second_halve_rss);
    vsz_increase_prcnt = ((double)diff_vsz)/((double)proc->avg_second_halve_vsz);
    
    if ( rss_increase_prcnt > (double)proc->p_config->settings.percent_diff_allow)
    {
        NDRX_LOG(log_warn, "pid %d leaky RSS: increase %lf%% allow: %d%%", 
                rss_increase_prcnt, proc->p_config->settings.percent_diff_allow);
        proc->status|=EXMEMCK_STATUS_LEAKY_RSS;
    }
    
    if ( vsz_increase_prcnt > (double)proc->p_config->settings.percent_diff_allow)
    {
        NDRX_LOG(log_warn, "pid %d leaky VSZ: increase %lf%% allow: %d%%", 
                vsz_increase_prcnt, proc->p_config->settings.percent_diff_allow);
        proc->status|=EXMEMCK_STATUS_LEAKY_VSZ;
    }
    
    NDRX_LOG(log_debug, "Process %d avg stats, first halve 4k pages: rss=%ld, vsz=%ld "
            "second halve: rss=%ld, vsz=%ld, rss_diff=%ld, vsz_diff=%ld, rss incr %d%%, "
            "vsz incr %d%%, rss_leak=%s, vsz_leak=%s (ps: %s)", 
            proc->pid,
            proc->avg_first_halve_rss, proc->avg_first_halve_vsz, 
            proc->avg_second_halve_rss, proc->avg_second_halve_vsz, 
            diff_rss, diff_vsz,
            rss_increase_prcnt, vsz_increase_prcnt,
            (proc->status&EXMEMCK_STATUS_LEAKY_RSS)?"yes":"no", 
            (proc->status&EXMEMCK_STATUS_LEAKY_VSZ)?"yes":"no",
            proc->psout);
    
    
    if ((proc->status&EXMEMCK_STATUS_LEAKY_RSS) ||
            (proc->status&EXMEMCK_STATUS_LEAKY_VSZ))
    {
        NDRX_LOG(log_error, "Process leaky! Invoke callback if set -> [%s]",
                proc->psout);
        if (NULL!=proc->p_config->settings.pf_proc_leaky)
        {
            proc->p_config->settings.pf_proc_leaky(proc);
        }
    }
    
out:
    NDRX_LOG(log_debug, "%s: returns");
}

/**
 * Lookup process by pid
 * @param pid
 * @return 
 */
exprivate exmemck_process_t* get_proc(pid_t pid)
{
    int pid_i = (int)pid;
    exmemck_process_t *ret=NULL;
    
    EXHASH_FIND_INT(M_proc, &pid_i, ret);    
    
    return ret;
}

/**
 * Get statistics for single process
 * @param pid
 * @return 
 */
expublic exmemck_process_t* ndrx_memck_getstats_single(pid_t pid)
{
    exmemck_process_t* ret;
    
    if (NULL!=(ret = get_proc(pid)))
    {
        calc_stat(ret);
    }
    
    return ret;
}

/**
 * Return statistics blocks, linked list...
 * @return 
 */
expublic exmemck_process_t* ndrx_memck_getstats(void)
{
    /* calculate the stats and return... */    
    exmemck_process_t* el, *elt;
    
    NDRX_LOG(log_debug, "%s - enter", __func__);
    
    EXHASH_ITER(hh, M_proc, el, elt)
    {
        calc_stat(el);
    }
    
    
    return M_proc;
}

/**
 * Reset statistics for process
 * @param mask
 * @return 
 */
expublic void ndrx_memck_reset(char *mask)
{
    exmemck_config_t * cfg = get_config(mask, EXFALSE, NULL, NULL);
    exmemck_process_t *el, *elt;
    
    if (NULL!=cfg)
    {
        EXHASH_ITER(hh, M_proc, el, elt)
        {
            if (el->p_config == cfg)
            {
                /* Matched process, resetting */
                NDRX_LOG(log_debug, "Resetting config for pid=%d (psout: [%s])",
                        el->pid, el->psout);
                
                NDRX_FREE(el->stats);
                el->nr_of_stats = 0;
            }
        }
    }
    
}

/**
 * Reset statistics for the PID
 * @param pid
 * @return 
 */
expublic void ndrx_memck_reset_pid(pid_t pid)    
{
    exmemck_process_t* proc = get_proc(pid);
    
    if (NULL!=proc)
    {
        /* Matched process, resetting */
        NDRX_LOG(log_debug, "Resetting config for pid=%d (psout: [%s])",
                proc->pid, proc->psout);

        NDRX_FREE(proc->stats);
        proc->nr_of_stats = 0;
    }   
}

/**
 * Run the one 
 * @return 
 */
expublic int ndrx_memck_tick(void)
{
    int ret = EXSUCCEED;
    
    /* List all processes that matches the mask */
    
    /* Check them against monitoring table */
    
    /* Check them against monitoring config */
    
    /* Add process statistics, if found in table, the add func shall create new
     * entry or append existing entry */
    
    /* If process is not running anymore, just report its stats via callback
     * and remove from the memory
     */
    
    /* Stats are calculated only on process exit or request by of stats func */
    
out:    
    return ret;
}

