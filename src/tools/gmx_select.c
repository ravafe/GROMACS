/*
 *
 *                This source code is part of
 *
 *                 G   R   O   M   A   C   S
 *
 *          GROningen MAchine for Chemical Simulations
 *
 * Written by David van der Spoel, Erik Lindahl, Berk Hess, and others.
 * Copyright (c) 1991-2000, University of Groningen, The Netherlands.
 * Copyright (c) 2001-2009, The GROMACS development team,
 * check out http://www.gromacs.org for more information.

 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * If you want to redistribute modifications, please consider that
 * scientific software is very special. Version control is crucial -
 * bugs must be traceable. We will be happy to consider code for
 * inclusion in the official distribution, but derived work must not
 * be called official GROMACS. Details are found in the README & COPYING
 * files - if they are missing, get the official version at www.gromacs.org.
 *
 * To help us fund GROMACS development, we humbly ask that you cite
 * the papers on the package - you can find them in the top README file.
 *
 * For more info, check our website at http://www.gromacs.org
 */
/*! \example gmx_select.c
 * \brief Utility/example program for writing out basic data for selections.
 */
/*! \file
 * \brief Utility program for writing out basic data for selections.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <copyrite.h>
#include <index.h>
#include <macros.h>
#include <smalloc.h>
#include <statutil.h>
#include <xvgr.h>

#include <trajana.h>

typedef struct
{
    bool                bDump;
    bool                bFracNorm;
    int                *size;
    FILE               *sfp;
    FILE               *cfp;
    FILE               *ifp;
    FILE               *mfp;
    gmx_ana_indexmap_t *mmap;
} t_dsdata;

static int
print_data(t_topology *top, t_trxframe *fr, t_pbc *pbc,
           int nr, gmx_ana_selection_t *sel[], void *data)
{
    t_dsdata           *d = (t_dsdata *)data;
    int                 g, i, b, mask;
    real                normfac;

    /* Write the sizes of the groups, possibly normalized */
    if (d->sfp)
    {
        fprintf(d->sfp, "%11.3f", fr->time);
        for (g = 0; g < nr; ++g)
        {
            normfac = d->bFracNorm ? 1.0 / sel[g]->cfrac : 1.0;
            fprintf(d->sfp, " %8.3f", sel[g]->p.nr * normfac / d->size[g]);
        }
        fprintf(d->sfp, "\n");
    }

    /* Write the covered fraction */
    if (d->cfp)
    {
        fprintf(d->cfp, "%11.3f", fr->time);
        for (g = 0; g < nr; ++g)
        {
            fprintf(d->cfp, " %6.4f", sel[g]->cfrac);
        }
        fprintf(d->cfp, "\n");
    }

    /* Write the actual indices */
    if (d->ifp)
    {
        if (!d->bDump)
        {
            fprintf(d->ifp, "%11.3f", fr->time);
        }
        for (g = 0; g < nr; ++g)
        {
            if (!d->bDump)
            {
                fprintf(d->ifp, " %d", sel[g]->p.nr);
            }
            for (i = 0; i < sel[g]->p.nr; ++i)
            {
                fprintf(d->ifp, " %d", sel[g]->p.m.mapid[i]+1);
            }
        }
        fprintf(d->ifp, "\n");
    }

    /* Write masks */
    if (d->mfp)
    {
        gmx_ana_indexmap_update(d->mmap, sel[0]->g, TRUE);
        if (!d->bDump)
        {
            fprintf(d->mfp, "%11.3f", fr->time);
        }
        for (b = 0; b < d->mmap->nr; ++b)
        {
            mask = (d->mmap->refid[b] == -1 ? 0 : 1);
            fprintf(d->mfp, d->bDump ? "%d\n" : " %d", mask);
        }
        if (!d->bDump)
        {
            fprintf(d->mfp, "\n");
        }
    }

    return 0;
}

int
gmx_select(int argc, char *argv[])
{
    const char *desc[] = {
        "g_select writes out basic data about dynamic selections.",
        "It can be used for some simple analyses, or the output can",
        "be combined with output from other programs and/or external",
        "analysis programs to calculate more complex things.",
        "Any combination of the output options is possible, but note",
        "that [TT]-om[tt] only operates on the first selection.[PAR]",
        "With [TT]-os[tt], calculates the number of positions in each",
        "selection for each frame. With [TT]-norm[tt], the output is",
        "between 0 and 1 and describes the fraction from the maximum",
        "number of positions (e.g., for selection 'resname RA and x < 5'",
        "the maximum number of positions is the number of atoms in",
        "RA residues). With [TT]-cfnorm[tt], the output is divided",
        "by the fraction covered by the selection.",
        "[TT]-norm[tt] and [TT]-cfnorm[tt] can be specified independently",
        "of one another.[PAR]",
        "With [TT]-oc[tt], the fraction covered by each selection is",
        "written out as a function of time.[PAR]",
        "With [TT]-oi[tt], the selected atoms/residues/molecules are",
        "written out as a function of time. In the output, the first",
        "column contains the frame time, the second contains the number",
        "of positions, followed by the atom/residue/molecule numbers.",
        "If more than one selection is specified, the size of the second",
        "group immediately follows the last number of the first group",
        "and so on. With [TT]-dump[tt], the frame time and the number",
        "of positions is omitted from the output. In this case, only one",
        "selection can be given.[PAR]",
        "With [TT]-om[tt], a mask is printed for the first selection",
        "as a function of time. Each line in the output corresponds to",
        "one frame, and contains either 0/1 for each atom/residue/molecule",
        "possibly selected. 1 stands for the atom/residue/molecule being",
        "selected for the current frame, 0 for not selected.",
        "With [TT]-dump[tt], the frame time is omitted from the output.",
    };

    bool                bDump     = FALSE;
    bool                bFracNorm = FALSE;
    bool                bTotNorm  = FALSE;
    t_pargs             pa[] = {
        {"-dump",   FALSE, etBOOL, {&bDump},
         "Do not print the frame time (-om, -oi) or the index size (-oi)"},
        {"-norm",   FALSE, etBOOL, {&bTotNorm},
         "Normalize by total number of positions with -os"},
        {"-cfnorm", FALSE, etBOOL, {&bFracNorm},
         "Normalize by covered fraction with -os"},
    };

    t_filenm            fnm[] = {
        {efXVG, "-os", "size.xvg",  ffOPTWR},
        {efXVG, "-oc", "cfrac.xvg", ffOPTWR},
        {efDAT, "-oi", "index.dat", ffOPTWR},
        {efDAT, "-om", "mask.dat",  ffOPTWR},
    };
#define NFILE asize(fnm)

    gmx_ana_traj_t       *trj;
    t_topology           *top;
    int                   ngrps;
    gmx_ana_selection_t **sel;
    char                **grpnames;
    t_dsdata              d;
    char                 *fnSize, *fnFrac, *fnIndex, *fnMask;
    int                   g;
    int                   rc;

    CopyRight(stderr, argv[0]);
    gmx_ana_traj_create(&trj, 0);
    gmx_ana_set_nanagrps(trj, -1);
    parse_trjana_args(trj, &argc, argv, 0,
                      NFILE, fnm, asize(pa), pa, asize(desc), desc, 0, NULL);
    gmx_ana_get_nanagrps(trj, &ngrps);
    gmx_ana_get_anagrps(trj, &sel);
    gmx_ana_init_coverfrac(trj, CFRAC_SOLIDANGLE);

    /* Get output file names */
    fnSize  = opt2fn_null("-os", NFILE, fnm);
    fnFrac  = opt2fn_null("-oc", NFILE, fnm);
    fnIndex = opt2fn_null("-oi", NFILE, fnm);
    fnMask  = opt2fn_null("-om", NFILE, fnm);
    /* Write out sizes if nothing specified */
    if (!fnFrac && !fnIndex && !fnMask)
    {
        fnSize = opt2fn("-os", NFILE, fnm);
    }

    if (bDump && ngrps > 1)
    {
        gmx_fatal(FARGS, "Only one index group allowed with -dump");
    }
    if (fnMask && ngrps > 1)
    {
        fprintf(stderr, "warning: the mask (-om) will only be written for the first group\n");
    }
    if (fnMask && !sel[0]->bDynamic)
    {
        fprintf(stderr, "warning: will not write the mask (-om) for a static selection\n");
        fnMask = NULL;
    }

    /* Initialize reference calculation for masks */
    if (fnMask)
    {
        gmx_ana_get_topology(trj, FALSE, &top, NULL);
        snew(d.mmap, 1);
        gmx_ana_indexmap_init(d.mmap, sel[0]->g, top, sel[0]->p.m.type);
    }

    /* Initialize calculation data */
    d.bDump     = bDump;
    d.bFracNorm = bFracNorm;
    snew(d.size,  ngrps);
    for (g = 0; g < ngrps; ++g)
    {
        d.size[g] = bTotNorm ? sel[g]->p.nr : 1;
    }

    /* Open output files */
    d.sfp = d.cfp = d.ifp = d.mfp = NULL;
    gmx_ana_get_grpnames(trj, &grpnames);
    if (fnSize)
    {
        d.sfp = xvgropen(fnSize, "Selection size", "Time (ps)", "Number");
        xvgr_selections(d.sfp, trj);
        xvgr_legend(d.sfp, ngrps, grpnames);
    }
    if (fnFrac)
    {
        d.cfp = xvgropen(fnFrac, "Covered fraction", "Time (ps)", "Fraction");
        xvgr_selections(d.cfp, trj);
        xvgr_legend(d.cfp, ngrps, grpnames);
    }
    if (fnIndex)
    {
        d.ifp = ffopen(fnIndex, "w");
        xvgr_selections(d.ifp, trj);
    }
    if (fnMask)
    {
        d.mfp = ffopen(fnMask, "w");
        xvgr_selections(d.mfp, trj);
    }

    /* Do the analysis and write out results */
    gmx_ana_do(trj, 0, &print_data, &d);

    /* Close the files */
    if (d.sfp)
    {
        fclose(d.sfp);
    }
    if (d.cfp)
    {
        fclose(d.cfp);
    }
    if (d.ifp)
    {
        fclose(d.ifp);
    }
    if (d.mfp)
    {
        fclose(d.mfp);
    }

    thanx(stderr);

    return 0;
}
