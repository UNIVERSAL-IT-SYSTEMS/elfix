/*
	fix-gnustack.c: this file is part of the elfix package
	Copyright (C) 2011  Anthony G. Basile

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <libgen.h>

#include <gelf.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <config.h>


void
print_help(char *v)
{
	printf(
		"Package Name : " PACKAGE_STRING "\n"
		"Bug Reports  : " PACKAGE_BUGREPORT "\n"
		"Program Name : %s\n"
		"Description  : Check for, or conditionally remove, executable flag from PT_GNU_STACK\n\n"
		"Usage        : %s -f ELF | -h\n"
		"options      :     Print out protection flags on PT_GNU_STACK\n"
		"             : -f  Remove X if WX flags are set on PT_GNU_STACK\n"
		"             : -h  Print out this help\n",
		basename(v),
		basename(v)
	);

	exit(EXIT_SUCCESS);
}


char *
parse_cmd_args( int c, char *v[], int *flagv  )
{
	int i, oc;

	if((c != 2)&&(c != 3))
		errx(EXIT_FAILURE, "Usage: %s -f ELF | -h", v[0]);

	*flagv = 0 ;
	while((oc = getopt(c, v,":fh")) != -1)
		switch(oc)
		{
			case 'f':
				*flagv = 1 ;
				break ;
			case 'h':
				print_help(v[0]);
				break;
			case '?':
			default:
				errx(EXIT_FAILURE, "option -%c is invalid: ignored.", optopt ) ;
		}

	return v[optind] ;
}



int
main( int argc, char *argv[])
{
	int fd, flagv;
	char *f_name;
	size_t i, phnum;

	Elf *elf;
	GElf_Phdr phdr;

	f_name = parse_cmd_args(argc, argv, &flagv);

	if(elf_version(EV_CURRENT) == EV_NONE)
		errx(EXIT_FAILURE, "Library out of date.");

	if(flagv)
	{
		if((fd = open(f_name, O_RDWR)) < 0)
			errx(EXIT_FAILURE, "open() fail.");
		if((elf = elf_begin(fd, ELF_C_RDWR_MMAP, NULL)) == NULL)
			errx(EXIT_FAILURE, "elf_begin() fail: %s", elf_errmsg(elf_errno()));
	}
	else
	{
		if((fd = open(f_name, O_RDONLY)) < 0)
			errx(EXIT_FAILURE, "open() fail.");
		if((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL)
			errx(EXIT_FAILURE, "elf_begin() fail: %s", elf_errmsg(elf_errno()));
	}

	if(elf_kind(elf) != ELF_K_ELF)
		errx(EXIT_FAILURE, "elf_kind() fail: this is not an elf file.");

	elf_getphdrnum(elf, &phnum);
	for(i=0; i<phnum; ++i)
	{
		if(gelf_getphdr(elf, i, &phdr) != &phdr)
			errx(EXIT_FAILURE, "gelf_getphdr(): %s", elf_errmsg(elf_errno()));

		if(phdr.p_type == PT_GNU_STACK)
		{
			printf("GNU_STACK: ");
			if( phdr.p_flags & PF_R ) printf("R");
			if( phdr.p_flags & PF_W ) printf("W");
			if( phdr.p_flags & PF_X ) printf("X");
			printf("\n");

			if(flagv && (phdr.p_flags & PF_W) && (phdr.p_flags & PF_X))
			{
				printf("W&X FOUND: X flag removed\n");
				phdr.p_flags ^= PF_X;
				if(!gelf_update_phdr(elf, i, &phdr))
					errx(EXIT_FAILURE, "gelf_update_phdr(): %s", elf_errmsg(elf_errno()));
			}
		}
	}

	elf_end(elf);
	close(fd);

	exit(EXIT_SUCCESS);
}
