/*
 * File:         arch/blackfin/kernel/module.c
 * Based on:
 * Author:
 *
 * Created:
 * Description:
 *
 * Modified:
 *               Copyright 2004-2006 Analog Devices Inc.
 *
 * Bugs:         Enter bugs at http://blackfin.uclinux.org/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#define pr_fmt(fmt) "module %s: " fmt

#include <linux/moduleloader.h>
#include <linux/elf.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <asm/dma.h>
#include <asm/cacheflush.h>

void *module_alloc(unsigned long size)
{
	if (size == 0)
		return NULL;
	return vmalloc(size);
}

/* Free memory returned from module_alloc */
void module_free(struct module *mod, void *module_region)
{
	vfree(module_region);
}

/* Transfer the section to the L1 memory */
int
module_frob_arch_sections(Elf_Ehdr * hdr, Elf_Shdr * sechdrs,
			  char *secstrings, struct module *mod)
{
	/*
	 * XXX: sechdrs are vmalloced in kernel/module.c
	 * and would be vfreed just after module is loaded,
	 * so we hack to keep the only information we needed
	 * in mod->arch to correctly free L1 I/D sram later.
	 * NOTE: this breaks the semantic of mod->arch structure.
	 */
	Elf_Shdr *s, *sechdrs_end = sechdrs + hdr->e_shnum;
	void *dest = NULL;

	for (s = sechdrs; s < sechdrs_end; ++s) {
		if ((strcmp(".l1.text", secstrings + s->sh_name) == 0) ||
		    ((strcmp(".text", secstrings + s->sh_name) == 0) &&
		     (hdr->e_flags & EF_BFIN_CODE_IN_L1) && (s->sh_size > 0))) {
			dest = l1_inst_sram_alloc(s->sh_size);
			mod->arch.text_l1 = dest;
			if (dest == NULL) {
				pr_err("L1 inst memory allocation failed\n",
					mod->name);
				return -1;
			}
			dma_memcpy(dest, (void *)s->sh_addr, s->sh_size);
			s->sh_flags &= ~SHF_ALLOC;
			s->sh_addr = (unsigned long)dest;
		}
		if ((strcmp(".l1.data", secstrings + s->sh_name) == 0) ||
		    ((strcmp(".data", secstrings + s->sh_name) == 0) &&
		     (hdr->e_flags & EF_BFIN_DATA_IN_L1) && (s->sh_size > 0))) {
			dest = l1_data_sram_alloc(s->sh_size);
			mod->arch.data_a_l1 = dest;
			if (dest == NULL) {
				pr_err("L1 data memory allocation failed\n",
					mod->name);
				return -1;
			}
			memcpy(dest, (void *)s->sh_addr, s->sh_size);
			s->sh_flags &= ~SHF_ALLOC;
			s->sh_addr = (unsigned long)dest;
		}
		if (strcmp(".l1.bss", secstrings + s->sh_name) == 0 ||
		    ((strcmp(".bss", secstrings + s->sh_name) == 0) &&
		     (hdr->e_flags & EF_BFIN_DATA_IN_L1) && (s->sh_size > 0))) {
			dest = l1_data_sram_zalloc(s->sh_size);
			mod->arch.bss_a_l1 = dest;
			if (dest == NULL) {
				pr_err("L1 data memory allocation failed\n",
					mod->name);
				return -1;
			}
			s->sh_flags &= ~SHF_ALLOC;
			s->sh_addr = (unsigned long)dest;
		}
		if (strcmp(".l1.data.B", secstrings + s->sh_name) == 0) {
			dest = l1_data_B_sram_alloc(s->sh_size);
			mod->arch.data_b_l1 = dest;
			if (dest == NULL) {
				pr_err("L1 data memory allocation failed\n",
					mod->name);
				return -1;
			}
			memcpy(dest, (void *)s->sh_addr, s->sh_size);
			s->sh_flags &= ~SHF_ALLOC;
			s->sh_addr = (unsigned long)dest;
		}
		if (strcmp(".l1.bss.B", secstrings + s->sh_name) == 0) {
			dest = l1_data_B_sram_alloc(s->sh_size);
			mod->arch.bss_b_l1 = dest;
			if (dest == NULL) {
				pr_err("L1 data memory allocation failed\n",
					mod->name);
				return -1;
			}
			memset(dest, 0, s->sh_size);
			s->sh_flags &= ~SHF_ALLOC;
			s->sh_addr = (unsigned long)dest;
		}
		if ((strcmp(".l2.text", secstrings + s->sh_name) == 0) ||
		    ((strcmp(".text", secstrings + s->sh_name) == 0) &&
		     (hdr->e_flags & EF_BFIN_CODE_IN_L2) && (s->sh_size > 0))) {
			dest = l2_sram_alloc(s->sh_size);
			mod->arch.text_l2 = dest;
			if (dest == NULL) {
				pr_err("L2 SRAM allocation failed\n",
					mod->name);
				return -1;
			}
			memcpy(dest, (void *)s->sh_addr, s->sh_size);
			s->sh_flags &= ~SHF_ALLOC;
			s->sh_addr = (unsigned long)dest;
		}
		if ((strcmp(".l2.data", secstrings + s->sh_name) == 0) ||
		    ((strcmp(".data", secstrings + s->sh_name) == 0) &&
		     (hdr->e_flags & EF_BFIN_DATA_IN_L2) && (s->sh_size > 0))) {
			dest = l2_sram_alloc(s->sh_size);
			mod->arch.data_l2 = dest;
			if (dest == NULL) {
				pr_err("L2 SRAM allocation failed\n",
					mod->name);
				return -1;
			}
			memcpy(dest, (void *)s->sh_addr, s->sh_size);
			s->sh_flags &= ~SHF_ALLOC;
			s->sh_addr = (unsigned long)dest;
		}
		if (strcmp(".l2.bss", secstrings + s->sh_name) == 0 ||
		    ((strcmp(".bss", secstrings + s->sh_name) == 0) &&
		     (hdr->e_flags & EF_BFIN_DATA_IN_L2) && (s->sh_size > 0))) {
			dest = l2_sram_zalloc(s->sh_size);
			mod->arch.bss_l2 = dest;
			if (dest == NULL) {
				pr_err("L2 SRAM allocation failed\n",
					mod->name);
				return -1;
			}
			s->sh_flags &= ~SHF_ALLOC;
			s->sh_addr = (unsigned long)dest;
		}
	}
	return 0;
}

int
apply_relocate(Elf_Shdr * sechdrs, const char *strtab,
	       unsigned int symindex, unsigned int relsec, struct module *me)
{
	pr_err(".rel unsupported\n", me->name);
	return -ENOEXEC;
}

/*************************************************************************/
/* FUNCTION : apply_relocate_add                                         */
/* ABSTRACT : Blackfin specific relocation handling for the loadable     */
/*            modules. Modules are expected to be .o files.              */
/*            Arithmetic relocations are handled.                        */
/*            We do not expect LSETUP to be split and hence is not       */
/*            handled.                                                   */
/*            R_BFIN_BYTE and R_BFIN_BYTE2 are also not handled as the   */
/*            gas does not generate it.                                  */
/*************************************************************************/
int
apply_relocate_add(Elf_Shdr * sechdrs, const char *strtab,
		   unsigned int symindex, unsigned int relsec,
		   struct module *mod)
{
	unsigned int i;
	unsigned short tmp;
	Elf32_Rela *rel = (void *)sechdrs[relsec].sh_addr;
	Elf32_Sym *sym;
	uint32_t *location32;
	uint16_t *location16;
	uint32_t value;

	pr_debug("applying relocate section %u to %u\n", mod->name,
		relsec, sechdrs[relsec].sh_info);
	for (i = 0; i < sechdrs[relsec].sh_size / sizeof(*rel); i++) {
		/* This is where to make the change */
		location16 =
		    (uint16_t *) (sechdrs[sechdrs[relsec].sh_info].sh_addr +
				  rel[i].r_offset);
		location32 = (uint32_t *) location16;
		/* This is the symbol it is referring to. Note that all
		   undefined symbols have been resolved. */
		sym = (Elf32_Sym *) sechdrs[symindex].sh_addr
		    + ELF32_R_SYM(rel[i].r_info);
		value = sym->st_value;
		value += rel[i].r_addend;

#ifdef CONFIG_SMP
		if ((unsigned long)location16 >= COREB_L1_DATA_A_START) {
			pr_err("cannot relocate in L1: %u (SMP kernel)",
				mod->name, ELF32_R_TYPE(rel[i].r_info));
			return -ENOEXEC;
		}
#endif

		pr_debug("location is %lx, value is %x type is %d\n",
			mod->name, (unsigned long)location32, value,
			ELF32_R_TYPE(rel[i].r_info));

		switch (ELF32_R_TYPE(rel[i].r_info)) {

		case R_BFIN_LUIMM16:
			tmp = (value & 0xffff);
			if ((unsigned long)location16 >= L1_CODE_START) {
				dma_memcpy(location16, &tmp, 2);
			} else
				*location16 = tmp;
			break;
		case R_BFIN_HUIMM16:
			tmp = ((value >> 16) & 0xffff);
			if ((unsigned long)location16 >= L1_CODE_START) {
				dma_memcpy(location16, &tmp, 2);
			} else
				*location16 = tmp;
			break;
		case R_BFIN_RIMM16:
			*location16 = (value & 0xffff);
			break;
		case R_BFIN_BYTE4_DATA:
			*location32 = value;
			break;
		case R_BFIN_PCREL24:
		case R_BFIN_PCREL24_JUMP_L:
		case R_BFIN_PCREL12_JUMP:
		case R_BFIN_PCREL12_JUMP_S:
		case R_BFIN_PCREL10:
			pr_err("unsupported relocation: %u (no -mlong-calls?)\n",
				mod->name, ELF32_R_TYPE(rel[i].r_info));
			return -ENOEXEC;
		default:
			pr_err("unknown relocation: %u\n", mod->name,
				ELF32_R_TYPE(rel[i].r_info));
			return -ENOEXEC;
		}
	}
	return 0;
}

int
module_finalize(const Elf_Ehdr * hdr,
		const Elf_Shdr * sechdrs, struct module *mod)
{
	unsigned int i, strindex = 0, symindex = 0;
	char *secstrings;
	long err = 0;

	secstrings = (void *)hdr + sechdrs[hdr->e_shstrndx].sh_offset;

	for (i = 1; i < hdr->e_shnum; i++) {
		/* Internal symbols and strings. */
		if (sechdrs[i].sh_type == SHT_SYMTAB) {
			symindex = i;
			strindex = sechdrs[i].sh_link;
		}
	}

	for (i = 1; i < hdr->e_shnum; i++) {
		const char *strtab = (char *)sechdrs[strindex].sh_addr;
		unsigned int info = sechdrs[i].sh_info;

		/* Not a valid relocation section? */
		if (info >= hdr->e_shnum)
			continue;

		if ((sechdrs[i].sh_type == SHT_RELA) &&
		    ((strcmp(".rela.l2.text", secstrings + sechdrs[i].sh_name) == 0) ||
		    (strcmp(".rela.l1.text", secstrings + sechdrs[i].sh_name) == 0) ||
		    ((strcmp(".rela.text", secstrings + sechdrs[i].sh_name) == 0) &&
			(hdr->e_flags & (EF_BFIN_CODE_IN_L1|EF_BFIN_CODE_IN_L2))))) {
			err = apply_relocate_add((Elf_Shdr *) sechdrs, strtab,
					   symindex, i, mod);
			if (err < 0)
				return -ENOEXEC;
		}
	}
	return 0;
}

void module_arch_cleanup(struct module *mod)
{
	l1_inst_sram_free(mod->arch.text_l1);
	l1_data_A_sram_free(mod->arch.data_a_l1);
	l1_data_A_sram_free(mod->arch.bss_a_l1);
	l1_data_B_sram_free(mod->arch.data_b_l1);
	l1_data_B_sram_free(mod->arch.bss_b_l1);
	l2_sram_free(mod->arch.text_l2);
	l2_sram_free(mod->arch.data_l2);
	l2_sram_free(mod->arch.bss_l2);
}
