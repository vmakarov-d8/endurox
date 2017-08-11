/* 
** Part of UBF library
** Utility for generating field header files.
** !!! THERE IS NO SUPPORT for multiple directories with in FLDTBLDIR!!!
** Also the usage of default `fld.tbl' is not supported, as seems to be un-needed
** feature.
**
** @file viewc.c
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

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <ndrx_config.h>
#include <atmi.h>
#include <atmi_int.h>
#include <sys_unix.h>

#include <ubf.h>
#include <ferror.h>
#include <fieldtable.h>
#include <fdatatype.h>

#include <ndrstandard.h>
#include <ndebug.h>
#include "viewc.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/
/*---------------------------Statics------------------------------------*/

/**
 * print usage
 * @param argc
 * @param argv
 * @return 
 */
exprivate void usage(char *progname)
{
    fprintf (stderr, "Usage: %s [-n] [-d viewdir] [-C] viewfile [viewfile ... ]\n", 
                        progname);
}

/**
 * Main entry point for view compiler
 */
int main(int argc, char **argv)
{
    int no_UBF = EXFALSE;
    char outdir[PATH_MAX+1]="./";
    int i;
    int c;
    int ret = EXSUCCEED;
    int lang_mode = HDR_C_LANG;
    char basename[PATH_MAX+1];
    char *p;
    int was_file = EXFALSE;
    char Vfile[PATH_MAX+1];
    opterr = 0;
    
    NDRX_BANNER;
    
    fprintf(stderr, "VIEW File Compiler\n\n");

    while ((c = getopt (argc, argv, "nd:Chm:")) != -1)
    {
        switch (c)
        {
            case 'n':
                NDRX_LOG(log_debug, "No UBF mapping processing...");
                no_UBF = EXTRUE;
                
                ndrx_view_loader_configure(no_UBF);
                        
                break;
            case 'd':
                NDRX_STRCPY_SAFE(outdir, optarg);
                NDRX_LOG(log_debug, "Changing view object output directory to: [%s]", 
                        outdir);
                break;
            case 'C':
                NDRX_LOG(log_warn, "Ignoring option C for COBOL");
                break;
            case 'm':
                lang_mode = atoi(optarg);
                NDRX_LOG(log_warn, "Language mode set to: %d", lang_mode);

                if (HDR_C_LANG!=lang_mode)
                {
                    NDRX_LOG(log_error, "Invalid language mode, currently %d supported only", 
                            HDR_C_LANG);
                    EXFAIL_OUT(ret);
                }

                break;
            case 'h':
                usage(argv[0]);
                EXFAIL_OUT(ret);
                break;
            case '?':
                if (optopt == 'c')
                {
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                }
                else if (isprint (optopt))
                {
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                }
                else
                {
                    fprintf (stderr,
                        "Unknown option character `\\x%x'.\n",
                        optopt);
                }
                EXFAIL_OUT(ret);
            default:
                NDRX_LOG(log_error, "Default case...");
                EXFAIL_OUT(ret);
        }
    }
        
    for (i = optind; i < argc; i++)
    {
        was_file = EXTRUE;
        NDRX_LOG (log_debug, "Processing VIEW file [%s]", argv[i]);
        
        /* Load the view file */
        if (EXSUCCEED!=ndrx_view_load_file(argv[i], EXFALSE))
        {
            NDRX_LOG(log_error, "Failed to load view file [%s]: %s", 
                    argv[i], tpstrerror(tperrno));
            EXFAIL_OUT(ret);
        }
        
        /* get the base name of output file */
        
        NDRX_STRCPY_SAFE(basename, argv[i]);
        p = strrchr(basename, '.');
        
        if (NULL!=p)
        {
            *p=EXEOS;
        }
        
        if (strlen(basename) == 0)
        {
            NDRX_LOG(log_error, "Invalid view file name passed on CLI...");
            EXFAIL_OUT(ret);
        }
        
        /* Plot the the header */
        if (HDR_C_LANG==lang_mode)
        {
            if (EXSUCCEED!=ndrx_view_plot_c_header(outdir, basename))
            {
                NDRX_LOG(log_error, "Failed to plot c header!");
                EXFAIL_OUT(ret);
            }
        
            /* Get the offset - generate object file & invoke */
            
            if (EXSUCCEED!=ndrx_view_generate_code(outdir, basename, argv[i], 
                    Vfile, no_UBF))
            {
                NDRX_LOG(log_error, "Failed to generate code or invoke compiler!");
                EXFAIL_OUT(ret);
            }
        }
        
        /* Unload the view files (remove from hashes) */
        ndrx_view_deleteall();
        
        NDRX_LOG(log_info, ">>> About to test object file...");
        
        if (EXSUCCEED!=ndrx_view_load_file(Vfile, EXTRUE))
        {
            NDRX_LOG(log_error, "!!! Failed to test object file [%s] !!!", Vfile);
            EXFAIL_OUT(ret);
        }
        
        ndrx_view_deleteall();
        
        
        NDRX_LOG(log_info, ">>> [%s] COMPILED & TESTED OK!", Vfile);
        
    }
    
    if (!was_file)
    {
        usage(argv[0]);
        EXFAIL_OUT(ret);
    }
    
out:
    /* So we need to load the view file now and generate header */
    return ret;
}


