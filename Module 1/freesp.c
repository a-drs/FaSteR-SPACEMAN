/*
 * Copyright (c) 2000-2001,2005 Silicon Graphics, Inc.
 * Copyright (c) 2012 Red Hat, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <xfs/xfs.h>
#include <xfs/xfs_types.h>
#include <xfs/command.h>
#include <linux/fs.h>
#include <linux/fiemap.h>
#include "init.h"
#include "space.h"

#ifndef FIEMAPFS_FLAG_FREESP
#define FIEMAPFS_FLAG_FREESP		0x80000000
#define FIEMAPFS_FLAG_FREESP_SIZE	0x40000000
#define FIEMAPFS_FLAG_FREESP_SIZE_HINT	0x20000000
#define	FIEMAPFS_FLAG_FREESP_CONTINUE   0x10000000

#define XFS_IOC_FIEMAPFS			_IOWR('X', 33, struct fiemap)
#endif

typedef struct histent
{
	int		low;
	int		high;
	long long	count;
	long long	blocks;
} histent_t;

static int		agcount;
static xfs_agnumber_t	*aglist;
static int		countflag;
static int		dumpflag;
static int		equalsize;
static histent_t	*hist;
static int		histcount;
static int		multsize;
static int		seen1;
static int		summaryflag;
static long long	totblocks;
static long long	totexts;

static cmdinfo_t freesp_cmd;

static void
addhistent(
	int	h)
{
	hist = realloc(hist, (histcount + 1) * sizeof(*hist));
	if (h == 0)
		h = 1;
	hist[histcount].low = h;
	hist[histcount].count = hist[histcount].blocks = 0;
	histcount++;
	if (h == 1)
		seen1 = 1;
}

static void				
addtohist(				
	xfs_agnumber_t	agno,	
	xfs_agblock_t	agbno,	
	off64_t		len)		
{
	int		i;

	if (dumpflag)		 
		printf("%8d %8d %8Zu\n", agno, agbno, len);
	totexts++;
	totblocks += len;
	for (i = 0; i < histcount; i++) {
		if (hist[i].high >= len) {
			hist[i].count++;
			hist[i].blocks += len;
			break;
		}
	}
}

static int
hcmp(
	const void	*a,
	const void	*b)
{
	return ((histent_t *)a)->low - ((histent_t *)b)->low;
}

static void
histinit(
	int	maxlen)
{
	int	i;

	if (equalsize) {
		for (i = 1; i < maxlen; i += equalsize)
			addhistent(i);
	} else if (multsize) {
		for (i = 1; i < maxlen; i *= multsize)
			addhistent(i);
	} else {
		if (!seen1)
			addhistent(1);
		qsort(hist, histcount, sizeof(*hist), hcmp);
	}
	for (i = 0; i < histcount; i++) {
		if (i < histcount - 1)
			hist[i].high = hist[i + 1].low - 1;
		else
			hist[i].high = maxlen;
	}
}

static void
printhist(void)
{
	int	i;
	printf("%7s %7s %7s %7s %6s\n",
		_("from"), _("to"), _("extents"), _("blocks"), _("pct"));
	for (i = 0; i < histcount; i++) {
		if (hist[i].count)
			printf("%7d %7d %7lld %7lld %6.2f\n", hist[i].low,
				hist[i].high, hist[i].count, hist[i].blocks,
				hist[i].blocks * 100.0 / totblocks);
	}
}

static int
inaglist(
	xfs_agnumber_t	agno)
{
	int		i;

	if (agcount == 0)
		return 1;
	for (i = 0; i < agcount; i++)
		if (aglist[i] == agno)
			return 1;
	return 0;
}

#define NR_EXTENTS 128

static void
scan_ag(
	xfs_agnumber_t	agno)
{
	struct fiemap	*fiemap;						
	off64_t		blocksize = file->geom.blocksize;			
	uint64_t	last_logical = agno * file->geom.agblocks * blocksize;  
	uint64_t	length = file->geom.agblocks * blocksize;		
	off64_t		fsbperag;						
	int		fiemap_flags;
	int		last = 0;
	int		map_size;

        last_logical = (off64_t)file->geom.agblocks * blocksize * agno;		
	length = (off64_t)file->geom.agblocks * blocksize;
	fsbperag = (off64_t)file->geom.agblocks * blocksize;			



	map_size = sizeof(struct fiemap) +
		   sizeof(struct fiemap_extent) * NR_EXTENTS;			
	fiemap = malloc(map_size);						
	if (!fiemap) {								
		fprintf(stderr, _("%s: fiemap malloc failed.\n"), progname);
		exitcode = 1;
		return;
	}
	if (countflag)								
		fiemap_flags = FIEMAPFS_FLAG_FREESP_SIZE;			
	else
		fiemap_flags = FIEMAPFS_FLAG_FREESP;

	while (!last) {
		struct fiemap_extent	*extent;
		xfs_agblock_t	agbno;						
		int		ret;
		int		i;

		memset(fiemap, 0, map_size);					
		fiemap->fm_flags = fiemap_flags;			
		fiemap_flags &= ~FIEMAPFS_FLAG_FREESP_CONTINUE;

		fiemap->fm_start = last_logical;			
		fiemap->fm_length = length;
		fiemap->fm_extent_count = NR_EXTENTS;

		ret = xfsctl(file->name,file->fd, XFS_IOC_FIEMAPFS, (unsigned long)fiemap);
		if (ret < 0) {
			fprintf(stderr, "%s: xfsctl(XFS_IOC_FIEMAPFS) [\"%s\"]: "
				"%s\n", progname, file->name, strerror(errno));
			free(fiemap);
			exitcode = 1;
			return;
		}

		/* No more extents to map, exit */
		if (!fiemap->fm_mapped_extents)
			break;

		for (i = 0; i < fiemap->fm_mapped_extents; i++) {
			off64_t			aglen;

			extent = &fiemap->fm_extents[i];				


			agbno = (extent->fe_physical - (fsbperag * agno)) /		
								blocksize;
			aglen = extent->fe_length / blocksize;				

			addtohist(agno, agbno, aglen);					


			if (extent->fe_flags & FIEMAP_EXTENT_LAST) {			
				last = 1;
				break;
			}
		}

		if (fiemap_flags == FIEMAPFS_FLAG_FREESP) {
			/* move our range past over what we just searched */
			last_logical = max(last_logical,
					extent->fe_logical + extent->fe_length);
		} else {
			/*
			 * we want to start the search from the current
			 * extent, but size ordered free space can be found
			 * anywhere in the range we asked for so we cannot move
			 * last_logical around. This means we need to give the
			 * search the last extent we've found back to the kernel
			 * for it to start it's search again. Move
			 * it to extent zero, and flag it as a continued call.
			 */
			memcpy(&fiemap->fm_extents[0], extent,
					sizeof(fiemap->fm_extents[0]));
			fiemap_flags |= FIEMAPFS_FLAG_FREESP_SIZE_HINT;
		}

	}
}
static void
aglistadd(
	char	*a)
{
	aglist = realloc(aglist, (agcount + 1) * sizeof(*aglist));
	aglist[agcount] = (xfs_agnumber_t)atoi(a);
	agcount++;
}

static int
init(
	int		argc,
	char		**argv)
{
	int		c;			
	int		speced = 0;		

	agcount = countflag = dumpflag = equalsize = multsize = optind = 0;
	histcount = seen1 = summaryflag = 0;
	totblocks = totexts = 0;
	aglist = NULL;
	hist = NULL;
	while ((c = getopt(argc, argv, "a:bcde:h:m:s")) != EOF) {  
		switch (c) {
		case 'a':
			aglistadd(optarg);		
			break;
		case 'b':
			if (speced)
				return 0;
			multsize = 2;
			speced = 1;
			break;
		case 'c':
			countflag = 1;
			break;
		case 'd':				
			dumpflag = 1;
			break;
		case 'e':				
			if (speced)
				return 0;
			equalsize = atoi(optarg);	
			speced = 1;
			break;
		case 'h':
			if (speced && !histcount)
				return 0;
			addhistent(atoi(optarg));
			speced = 1;
			break;
		case 'm':
			if (speced)
				return 0;
			multsize = atoi(optarg);
			speced = 1;
			break;
		case 's':
			summaryflag = 1;		
			break;
		case '?':
			return 0;
		}
	}
	if (optind != argc)
		return 0;
	if (!speced)
		multsize = 2;
	histinit(file->geom.agblocks); 
	return 1;                      
}

/*
 * Report on freespace usage in xfs filesystem.
 */
static int
freesp_f(
	int		argc,
	char		**argv)
{
	xfs_agnumber_t	agno;		

	if (!init(argc, argv))
		return 0;

	if (dumpflag)
		printf("%8s %8s %8s\n", "agno", "agbno", "len");	

	for (agno = 0; agno < file->geom.agcount; agno++)  {
		if (inaglist(agno))
			scan_ag(agno);
	}
	if (histcount)
		printhist();		
	if (summaryflag) {
		printf(_("total free extents %lld\n"), totexts);
		printf(_("total free blocks %lld\n"), totblocks);
		printf(_("average free extent size %g\n"),
			(double)totblocks / (double)totexts);
	}
	if (aglist)
		free(aglist);
	if (hist)
		free(hist);
	return 0;
}

static void
freesp_help(void)
{
	printf(_(
"\n"
"Examine filesystem free space\n"
"\n"
"Options: [-bcds] [-a agno] [-e bsize] [-h h1]... [-m bmult]\n"
"\n"
" -b -- binary histogram bin size\n"
" -c -- scan the by-count (size) ordered freespace tree\n"
" -d -- debug output\n"
" -s -- emit freespace summary information\n"
" -a agno -- scan only the given AG agno\n"
" -e bsize -- use fixed histogram bin size of bsize\n"
" -h h1 -- use custom histogram bin size of h1. Multiple specifications allowed.\n"
" -m bmult -- use histogram bin size multiplier of bmult\n"
"\n"));

}

void
freesp_init(void)
{
	freesp_cmd.name = "freesp";
	freesp_cmd.altname = "fsp";
	freesp_cmd.cfunc = freesp_f;
	freesp_cmd.argmin = 0;
	freesp_cmd.argmax = -1;
	freesp_cmd.args = "[-bcds] [-a agno] [-e bsize] [-h h1]... [-m bmult]\n";
	freesp_cmd.flags = CMD_FLAG_GLOBAL;
	freesp_cmd.oneline = _("Examine filesystem free space");
	freesp_cmd.help = freesp_help;

	add_command(&freesp_cmd);
}

