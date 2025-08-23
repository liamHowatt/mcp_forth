#include "mcp_forth.h"
#include <elf.h>

#define SHSTRTAB "\0.shstrtab\0.symtab\0.strtab\0.text\0.data"
#define STRTAB "\0cont\0code"

typedef struct {
    Elf32_Ehdr ehdr;
    Elf32_Phdr phdrs[2];
    Elf32_Shdr shdrs[6];
    Elf32_Sym symtab[3];
    char shstrtab[sizeof(SHSTRTAB)];
    char strtab[sizeof(STRTAB)];
    __attribute__((aligned(4))) uint8_t cont[];
} m4_elf_nuttx_t;

int m4_elf_nuttx_size(void)
{
    return offsetof(m4_elf_nuttx_t, cont);
}

void m4_elf_nuttx(void * aligned_elf_dst, m4_arch_t machine, int cont_len, int code_len)
{
    m4_elf_nuttx_t * e = aligned_elf_dst;

    uint16_t elf_machine;
    switch(machine) {
        case M4_ARCH_X86_32:
            elf_machine = EM_386;
            break;
        case M4_ARCH_ESP32S3:
            elf_machine = EM_XTENSA;
            break;
        default:
            assert(0);
    }

    memset(e, 0, m4_elf_nuttx_size());

    e->ehdr.e_ident[EI_MAG0] = ELFMAG0;
    e->ehdr.e_ident[EI_MAG1] = ELFMAG1;
    e->ehdr.e_ident[EI_MAG2] = ELFMAG2;
    e->ehdr.e_ident[EI_MAG3] = ELFMAG3;
    e->ehdr.e_ident[EI_CLASS] = ELFCLASS32;
    e->ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
    e->ehdr.e_ident[EI_VERSION] = EV_CURRENT;
    e->ehdr.e_ident[EI_OSABI] = ELFOSABI_SYSV;

    e->ehdr.e_type = ET_REL; // relocatable object
    e->ehdr.e_machine = elf_machine;
    e->ehdr.e_version = EV_CURRENT;
    e->ehdr.e_entry = 0;
    e->ehdr.e_phoff = offsetof(m4_elf_nuttx_t, phdrs);
    e->ehdr.e_shoff = offsetof(m4_elf_nuttx_t, shdrs);
    e->ehdr.e_flags = 0;
    e->ehdr.e_ehsize = sizeof(e->ehdr);
    e->ehdr.e_phentsize = sizeof(e->phdrs[0]);
    e->ehdr.e_phnum = sizeof(e->phdrs) / sizeof(e->phdrs[0]);
    e->ehdr.e_shentsize = sizeof(e->shdrs[0]);
    e->ehdr.e_shnum = sizeof(e->shdrs) / sizeof(e->shdrs[0]);
    e->ehdr.e_shstrndx = 1;

    e->phdrs[0].p_type = PT_LOAD;
    e->phdrs[0].p_offset = offsetof(m4_elf_nuttx_t, cont);
    e->phdrs[0].p_vaddr = 0;
    e->phdrs[0].p_paddr = 0;
    e->phdrs[0].p_filesz = cont_len;
    e->phdrs[0].p_memsz = cont_len;
    e->phdrs[0].p_flags = PF_W | PF_R;
    e->phdrs[0].p_align = 4;

    e->phdrs[1].p_type = PT_LOAD;
    e->phdrs[1].p_offset = offsetof(m4_elf_nuttx_t, cont) + cont_len;
    e->phdrs[1].p_vaddr = 0;
    e->phdrs[1].p_paddr = 0;
    e->phdrs[1].p_filesz = code_len;
    e->phdrs[1].p_memsz = code_len;
    e->phdrs[1].p_flags = PF_X;
    e->phdrs[1].p_align = 4;

    e->shdrs[1].sh_name = 1;
    e->shdrs[1].sh_type = SHT_STRTAB;
    e->shdrs[1].sh_flags = 0;
    e->shdrs[1].sh_addr = offsetof(m4_elf_nuttx_t, shstrtab);
    e->shdrs[1].sh_offset = offsetof(m4_elf_nuttx_t, shstrtab);
    e->shdrs[1].sh_size = sizeof(e->shstrtab);
    e->shdrs[1].sh_link = 0;
    e->shdrs[1].sh_info = 0;
    e->shdrs[1].sh_addralign = 1;
    e->shdrs[1].sh_entsize = 0;

    e->shdrs[2].sh_name = 11;
    e->shdrs[2].sh_type = SHT_SYMTAB;
    e->shdrs[2].sh_flags = 0;
    e->shdrs[2].sh_addr = offsetof(m4_elf_nuttx_t, symtab);
    e->shdrs[2].sh_offset = offsetof(m4_elf_nuttx_t, symtab);
    e->shdrs[2].sh_size = sizeof(e->symtab);
    e->shdrs[2].sh_link = 3;
    e->shdrs[2].sh_info = 1;
    e->shdrs[2].sh_addralign = 4;
    e->shdrs[2].sh_entsize = sizeof(e->symtab[0]);

    e->shdrs[3].sh_name = 19;
    e->shdrs[3].sh_type = SHT_STRTAB;
    e->shdrs[3].sh_flags = 0;
    e->shdrs[3].sh_addr = offsetof(m4_elf_nuttx_t, strtab);
    e->shdrs[3].sh_offset = offsetof(m4_elf_nuttx_t, strtab);
    e->shdrs[3].sh_size = sizeof(e->strtab);
    e->shdrs[3].sh_link = 0;
    e->shdrs[3].sh_info = 0;
    e->shdrs[3].sh_addralign = 1;
    e->shdrs[3].sh_entsize = 0;

    e->shdrs[4].sh_name = 33;
    e->shdrs[4].sh_type = SHT_PROGBITS;
    e->shdrs[4].sh_flags = SHF_ALLOC | SHF_WRITE;
    e->shdrs[4].sh_addr = 0;
    e->shdrs[4].sh_offset = offsetof(m4_elf_nuttx_t, cont);
    e->shdrs[4].sh_size = cont_len;
    e->shdrs[4].sh_link = 0;
    e->shdrs[4].sh_info = 0;
    e->shdrs[4].sh_addralign = 4;
    e->shdrs[4].sh_entsize = 0;

    e->shdrs[5].sh_name = 27;
    e->shdrs[5].sh_type = SHT_PROGBITS;
    e->shdrs[5].sh_flags = SHF_ALLOC | SHF_EXECINSTR;
    e->shdrs[5].sh_addr = 0;
    e->shdrs[5].sh_offset = offsetof(m4_elf_nuttx_t, cont) + cont_len;
    e->shdrs[5].sh_size = code_len;
    e->shdrs[5].sh_link = 0;
    e->shdrs[5].sh_info = 0;
    e->shdrs[5].sh_addralign = 4;
    e->shdrs[5].sh_entsize = 0;

    e->symtab[1].st_name = 1;
    e->symtab[1].st_value = 0;
    e->symtab[1].st_size = cont_len;
    e->symtab[1].st_info = ELF32_ST_INFO(STB_GLOBAL, STT_OBJECT);
    e->symtab[1].st_other = STV_DEFAULT;
    e->symtab[1].st_shndx = 4;

    e->symtab[2].st_name = 6;
    e->symtab[2].st_value = 0;
    e->symtab[2].st_size = code_len;
    e->symtab[2].st_info = ELF32_ST_INFO(STB_GLOBAL, STT_FUNC);
    e->symtab[2].st_other = STV_DEFAULT;
    e->symtab[2].st_shndx = 5;

    memcpy(e->shstrtab, SHSTRTAB, sizeof(e->shstrtab));

    memcpy(e->strtab, STRTAB, sizeof(e->strtab));
}
