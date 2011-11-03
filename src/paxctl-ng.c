/*
	paxctl-ng.c: get/set pax flags on an ELF object
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <error.h>
#include <libgen.h>

#include <gelf.h>
#include <attr/xattr.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <config.h>


#define PAX_NAMESPACE	"user.pax"
#define BUF_SIZE	8
#define FILE_NAME_SIZE	32768

#define CREATE_XT_FLAGS_SECURE		1
#define CREATE_XT_FLAGS_DEFAULT		2
#define COPY_PT_TO_XT_FLAGS		3
#define COPY_XT_TO_PT_FLAGS		4

void
print_help_exit(char *v)
{
	printf(
		"\n"
		"Package Name : " PACKAGE_STRING "\n"
		"Bug Reports  : " PACKAGE_BUGREPORT "\n"
		"Program Name : %s\n"
		"Description  : Get or set pax flags on an ELF object\n\n"
		"Usage        : %s -PpEeMmRrXxSsv ELF | -Zv ELF | -zv ELF\n"
		"             : %s -Cv ELF | -cv ELF | Fv ELF | -fv ELF\n"
		"             : %s -v ELF | -h\n\n"
		"Options      : -P enable PAGEEXEC\t-p disable  PAGEEXEC\n"
		"             : -S enable SEGMEXEC\t-s disable  SEGMEXEC\n"
		"             : -M enable MPROTECT\t-m disable  MPROTECT\n"
		"             : -E enable EMUTRAMP\t-e disable  EMUTRAMP\n"
		"             : -R enable RANDMMAP\t-r disable  RANDMMAP\n"
		"             : -X enable RANDEXEC\t-x disable  RANDEXEC\n"
		"             : -Z most secure settings\t-z all default settings\n"
		"             : -C create XT_PAX with most secure setting\n"
		"             : -c create XT_PAX all default settings\n"
		"             : -F copy PT_PAX to XT_PAX\n"
		"             : -f copy XT_PAX to PT_PAX\n"
		"             : -v view the flags, along with any accompanying operation\n"
		"             : -h print out this help\n\n"
		"Note         :  If both enabling and disabling flags are set, the default - is used\n\n",
		basename(v),
		basename(v),
		basename(v),
		basename(v)
	);

	exit(EXIT_SUCCESS);
}


void
parse_cmd_args(int argc, char *argv[], uint16_t *pax_flags, int *view_flags, int *cp_flags,
	int *begin, int *end)
{
	int i, oc;
	int compat, solitaire;

	compat = 0;
	solitaire = 0;
	*pax_flags = 0;
	*view_flags = 0;
	*cp_flags = 0; 
	while((oc = getopt(argc, argv,":PpEeMmRrXxSsZzCcFfvh")) != -1)
	{
		switch(oc)
		{
			case 'P':
				*pax_flags |= PF_PAGEEXEC;
				compat |= 1;
				break;
			case 'p':
				*pax_flags |= PF_NOPAGEEXEC;
				compat |= 1;
				break ;
			case 'S':
				*pax_flags |= PF_SEGMEXEC;
				compat |= 1;
				break;
			case 's':
				*pax_flags |= PF_NOSEGMEXEC;
				compat |= 1;
				break ;
			case 'M':
				*pax_flags |= PF_MPROTECT;
				compat |= 1;
				break;
			case 'm':
				*pax_flags |= PF_NOMPROTECT;
				compat |= 1;
				break ;
			case 'E':
				*pax_flags |= PF_EMUTRAMP;
				compat |= 1;
				break;
			case 'e':
				*pax_flags |= PF_NOEMUTRAMP;
				compat |= 1;
				break ;
			case 'R':
				*pax_flags |= PF_RANDMMAP;
				compat |= 1;
				break;
			case 'r':
				*pax_flags |= PF_NORANDMMAP;
				compat |= 1;
				break ;
			case 'X':
				*pax_flags |= PF_RANDEXEC;
				compat |= 1;
				break;
			case 'x':
				*pax_flags |= PF_NORANDEXEC;
				compat |= 1;
				break ;
			case 'Z':
				*pax_flags = PF_PAGEEXEC | PF_SEGMEXEC | PF_MPROTECT |
					PF_NOEMUTRAMP | PF_RANDMMAP | PF_NORANDEXEC;
				solitaire += 1;
				break ;
			case 'z':
				*pax_flags = PF_PAGEEXEC | PF_NOPAGEEXEC | PF_SEGMEXEC | PF_NOSEGMEXEC |
					PF_MPROTECT | PF_NOMPROTECT | PF_EMUTRAMP | PF_NOEMUTRAMP |
					PF_RANDMMAP | PF_NORANDMMAP | PF_RANDEXEC | PF_NORANDEXEC;
				solitaire += 1;
				break;
			case 'C':
				solitaire += 1;
				*cp_flags = CREATE_XT_FLAGS_SECURE;
				break;
			case 'c':
				solitaire += 1;
				*cp_flags = CREATE_XT_FLAGS_DEFAULT;
				break;
			case 'F':
				solitaire += 1;
				*cp_flags = COPY_PT_TO_XT_FLAGS;
				break;
			case 'f':
				solitaire += 1;
				*cp_flags = COPY_XT_TO_PT_FLAGS;
				break;
			case 'v':
				*view_flags = 1;
				break;
			case 'h':
				print_help_exit(argv[0]);
				break;
			case '?':
			default:
				error(EXIT_FAILURE, 0, "option -%c is invalid: ignored.", optopt ) ;
		}
	}

	if(	((compat == 1 && solitaire == 0) ||
		 (compat == 0 && solitaire == 1) ||
		 (compat == 0 && solitaire == 0 && *view_flags == 1)
		) && argv[optind] != NULL)
	{
		*begin = optind;
		*end = argc;
	}
	else
		print_help_exit(argv[0]);
}


uint16_t
get_pt_flags(int fd)
{
	Elf *elf;
	GElf_Phdr phdr;
	size_t i, phnum;

	uint16_t pt_flags = UINT16_MAX;

	if(elf_version(EV_CURRENT) == EV_NONE)
	{
		printf("\tELF ERROR: Library out of date.\n");
		return pt_flags;
	}

	if((elf = elf_begin(fd, ELF_C_READ_MMAP, NULL)) == NULL)
	{
		printf("\tELF ERROR: elf_begin() fail: %s\n", elf_errmsg(elf_errno()));
		return pt_flags;
	}

	if(elf_kind(elf) != ELF_K_ELF)
	{
		elf_end(elf);
		printf("\tELF ERROR: elf_kind() fail: this is not an elf file.\n");
		return pt_flags;
	}

	elf_getphdrnum(elf, &phnum);

	for(i=0; i<phnum; i++)
	{
		if(gelf_getphdr(elf, i, &phdr) != &phdr)
		{
			elf_end(elf);
			printf("\tELF ERROR: gelf_getphdr(): %s\n", elf_errmsg(elf_errno()));
			return pt_flags;
		}

		if(phdr.p_type == PT_PAX_FLAGS)
			pt_flags = phdr.p_flags;
	}

	elf_end(elf);
	return pt_flags;
}


uint16_t
get_xt_flags(int fd)
{
	uint16_t xt_flags = UINT16_MAX;

	fgetxattr(fd, PAX_NAMESPACE, &xt_flags, sizeof(uint16_t));
	return xt_flags;
}


void
bin2string(uint16_t flags, char *buf)
{
	buf[0] = flags & PF_PAGEEXEC ? 'P' :
		flags & PF_NOPAGEEXEC ? 'p' : '-' ;

	buf[1] = flags & PF_SEGMEXEC   ? 'S' : 
		flags & PF_NOSEGMEXEC ? 's' : '-';

	buf[2] = flags & PF_MPROTECT   ? 'M' :
		flags & PF_NOMPROTECT ? 'm' : '-';

	buf[3] = flags & PF_EMUTRAMP   ? 'E' :
		flags & PF_NOEMUTRAMP ? 'e' : '-';

	buf[4] = flags & PF_RANDMMAP   ? 'R' :
		flags & PF_NORANDMMAP ? 'r' : '-';

	buf[5] = flags & PF_RANDEXEC   ? 'X' :
		flags & PF_NORANDEXEC ? 'x' : '-';
}


void
print_flags(int fd)
{
	uint16_t flags;
	char buf[BUF_SIZE];

	flags = get_pt_flags(fd);
	if( flags == UINT16_MAX )
		printf("\tPT_PAX: not found\n");
	else
	{
		memset(buf, 0, BUF_SIZE);
		bin2string(flags, buf);
		printf("\tPT_PAX: %s\n", buf);
	}

	flags = get_xt_flags(fd);
	if( flags == UINT16_MAX )
		printf("\tXT_PAX: not found\n");
	else
	{
		memset(buf, 0, BUF_SIZE);
		bin2string(flags, buf);
		printf("\tXT_PAX: %s\n", buf);
	}

	printf("\n");
}



uint16_t
update_flags(uint16_t flags, uint16_t pax_flags)
{
	//PAGEEXEC
	if(pax_flags & PF_PAGEEXEC)
	{
		flags |= PF_PAGEEXEC;
		flags &= ~PF_NOPAGEEXEC;
	}
	if(pax_flags & PF_NOPAGEEXEC)
	{
		flags &= ~PF_PAGEEXEC;
		flags |= PF_NOPAGEEXEC;
	}
	if((pax_flags & PF_PAGEEXEC) && (pax_flags & PF_NOPAGEEXEC))
	{
		flags &= ~PF_PAGEEXEC;
		flags &= ~PF_NOPAGEEXEC;
	}

	//SEGMEXEC
	if(pax_flags & PF_SEGMEXEC)
	{
		flags |= PF_SEGMEXEC;
		flags &= ~PF_NOSEGMEXEC;
	}
	if(pax_flags & PF_NOSEGMEXEC)
	{
		flags &= ~PF_SEGMEXEC;
		flags |= PF_NOSEGMEXEC;
	}
	if((pax_flags & PF_SEGMEXEC) && (pax_flags & PF_NOSEGMEXEC))
	{
		flags &= ~PF_SEGMEXEC;
		flags &= ~PF_NOSEGMEXEC;
	}

	//MPROTECT
	if(pax_flags & PF_MPROTECT)
	{
		flags |= PF_MPROTECT;
		flags &= ~PF_NOMPROTECT;
	}
	if(pax_flags & PF_NOMPROTECT)
	{
		flags &= ~PF_MPROTECT;
		flags |= PF_NOMPROTECT;
	}
	if((pax_flags & PF_MPROTECT) && (pax_flags & PF_NOMPROTECT))
	{
		flags &= ~PF_MPROTECT;
		flags &= ~PF_NOMPROTECT;
	}

	//EMUTRAMP
	if(pax_flags & PF_EMUTRAMP)
	{
		flags |= PF_EMUTRAMP;
		flags &= ~PF_NOEMUTRAMP;
	}
	if(pax_flags & PF_NOEMUTRAMP)
	{
		flags &= ~PF_EMUTRAMP;
		flags |= PF_NOEMUTRAMP;
	}
	if((pax_flags & PF_EMUTRAMP) && (pax_flags & PF_NOEMUTRAMP))
	{
		flags &= ~PF_EMUTRAMP;
		flags &= ~PF_NOEMUTRAMP;
	}

	//RANDMMAP
	if(pax_flags & PF_RANDMMAP)
	{
		flags |= PF_RANDMMAP;
		flags &= ~PF_NORANDMMAP;
	}
	if(pax_flags & PF_NORANDMMAP)
	{
		flags &= ~PF_RANDMMAP;
		flags |= PF_NORANDMMAP;
	}
	if((pax_flags & PF_RANDMMAP) && (pax_flags & PF_NORANDMMAP))
	{
		flags &= ~PF_RANDMMAP;
		flags &= ~PF_NORANDMMAP;
	}

	//RANDEXEC
	if(pax_flags & PF_RANDEXEC)
	{
		flags |= PF_RANDEXEC;
		flags &= ~PF_NORANDEXEC;
	}
	if(pax_flags & PF_NORANDEXEC)
	{
		flags &= ~PF_RANDEXEC;
		flags |= PF_NORANDEXEC;
	}
	if((pax_flags & PF_RANDEXEC) && (pax_flags & PF_NORANDEXEC))
	{
		flags &= ~PF_RANDEXEC;
		flags &= ~PF_NORANDEXEC;
	}

	return flags;
}


void
set_pt_flags(int fd, uint16_t pt_flags)
{
	Elf *elf;
	GElf_Phdr phdr;
	size_t i, phnum;

	if(elf_version(EV_CURRENT) == EV_NONE)
	{
		printf("\tELF ERROR: Library out of date.\n");
		return;
	}

	if((elf = elf_begin(fd, ELF_C_RDWR_MMAP, NULL)) == NULL)
	{
		printf("\tELF ERROR: elf_begin() fail: %s\n", elf_errmsg(elf_errno()));
		return;
	}

	if(elf_kind(elf) != ELF_K_ELF)
	{
		elf_end(elf);
		printf("\tELF ERROR: elf_kind() fail: this is not an elf file.\n");
		return; 
	}

	elf_getphdrnum(elf, &phnum);

	for(i=0; i<phnum; i++)
	{
		if(gelf_getphdr(elf, i, &phdr) != &phdr)
		{
			elf_end(elf);
			printf("\tELF ERROR: gelf_getphdr(): %s\n", elf_errmsg(elf_errno()));
			return;
		}

		if(phdr.p_type == PT_PAX_FLAGS)
		{
			phdr.p_flags = pt_flags;

			if(!gelf_update_phdr(elf, i, &phdr))
			{
				elf_end(elf);
				printf("\tELF ERROR: gelf_update_phdr(): %s", elf_errmsg(elf_errno()));
			}
		}
	}

	elf_end(elf);
}


void
set_xt_flags(int fd, uint16_t xt_flags)
{
	fsetxattr(fd, PAX_NAMESPACE, &xt_flags, sizeof(uint16_t), XATTR_REPLACE);
}


void
set_flags(int fd, uint16_t *pax_flags, int rdwr_pt_pax)
{
	uint16_t flags;

	if(rdwr_pt_pax)
	{
		flags = get_pt_flags(fd);
		if( flags == UINT16_MAX )
			flags = PF_NOEMUTRAMP | PF_NORANDEXEC;
		flags = update_flags( flags, *pax_flags);
		set_pt_flags(fd, flags);
	}

	flags = get_xt_flags(fd);
	if( flags == UINT16_MAX )
		flags = PF_NOEMUTRAMP | PF_NORANDEXEC;
	flags = update_flags( flags, *pax_flags);
	set_xt_flags(fd, flags);
}


void
create_xt_flags(fd, cp_flags)
{
	uint16_t xt_flags;

	if(cp_flags == 1)
		xt_flags = PF_PAGEEXEC | PF_SEGMEXEC | PF_MPROTECT |
			PF_NOEMUTRAMP | PF_RANDMMAP | PF_NORANDEXEC;
	else if(cp_flags == 2)
		xt_flags = 0;

	fsetxattr(fd, PAX_NAMESPACE, &xt_flags, sizeof(uint16_t), XATTR_CREATE);
}


void
copy_xt_flags(fd, cp_flags)
{
	uint16_t flags;
	if(cp_flags == 3)
	{
		flags = get_pt_flags(fd);
		set_xt_flags(fd, flags);
	}
	else if(cp_flags == 4)
	{
		flags = get_xt_flags(fd);
		set_pt_flags(fd, flags);
	}
}


int
main( int argc, char *argv[])
{
	int fd, fi;
	uint16_t pax_flags;
	int view_flags, cp_flags, begin, end;
	int rdwr_pt_pax = 1;

	parse_cmd_args(argc, argv, &pax_flags, &view_flags, &cp_flags, &begin, &end);

	for(fi = begin; fi < end; fi++)
	{
		printf("%s:\n", argv[fi]);

		if((fd = open(argv[fi], O_RDWR)) < 0)
		{
			rdwr_pt_pax = 0;
			printf("\topen(O_RDWR) failed: cannot change PT_PAX flags\n");
			if((fd = open(argv[fi], O_RDONLY)) < 0)
				error(EXIT_FAILURE, 0, "open() failed");
		}

		if(cp_flags == CREATE_XT_FLAGS_SECURE || cp_flags == CREATE_XT_FLAGS_DEFAULT)
			create_xt_flags(fd, cp_flags);

		if(cp_flags == COPY_PT_TO_XT_FLAGS || (cp_flags == COPY_XT_TO_PT_FLAGS && rdwr_pt_pax))
			copy_xt_flags(fd, cp_flags);

		if(pax_flags != 1)
			set_flags(fd, &pax_flags, rdwr_pt_pax);

		if(view_flags == 1)
			print_flags(fd);

		close(fd);
	}
}
