#include "mcp_forth.h"
#include <elf.h>

#define SHSTRTAB "\0.shstrtab\0.symtab\0.strtab\0.text\0.dynamic\0.dynstr\0.dynsym\0.hash"
#define STRTAB "\0cont"

typedef struct {
    Elf32_Ehdr ehdr;
    Elf32_Phdr phdrs[3];
    Elf32_Shdr shdrs[9];
    Elf32_Sym symtab[2];
    Elf32_Dyn dyn[6];
    struct {
        uint32_t nbucket;
        uint32_t nchain;
        uint32_t bucket[1];
        uint32_t chain[2];
    } hash;
    char shstrtab[sizeof(SHSTRTAB)];
    char strtab[sizeof(STRTAB)];
    __attribute__((aligned(0x1000))) uint8_t cont[];
} m4_elf_linux_t;

int m4_elf_linux_size(void)
{
    return offsetof(m4_elf_linux_t, cont);
}

void m4_elf_linux(void * aligned_elf_dst, m4_arch_t machine, int bin_len)
{
    m4_elf_linux_t * e = aligned_elf_dst;

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

    memset(e, 0, m4_elf_linux_size());

    e->ehdr.e_ident[EI_MAG0] = ELFMAG0;
    e->ehdr.e_ident[EI_MAG1] = ELFMAG1;
    e->ehdr.e_ident[EI_MAG2] = ELFMAG2;
    e->ehdr.e_ident[EI_MAG3] = ELFMAG3;
    e->ehdr.e_ident[EI_CLASS] = ELFCLASS32;
    e->ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
    e->ehdr.e_ident[EI_VERSION] = EV_CURRENT;
    e->ehdr.e_ident[EI_OSABI] = ELFOSABI_SYSV;
    e->ehdr.e_ident[EI_ABIVERSION] = 0;

    e->ehdr.e_type = ET_DYN; // shared object
    e->ehdr.e_machine = elf_machine;
    e->ehdr.e_version = EV_CURRENT;
    e->ehdr.e_entry = 0;
    e->ehdr.e_phoff = offsetof(m4_elf_linux_t, phdrs);
    e->ehdr.e_shoff = offsetof(m4_elf_linux_t, shdrs);
    e->ehdr.e_flags = 0;
    e->ehdr.e_ehsize = sizeof(e->ehdr);
    e->ehdr.e_phentsize = sizeof(e->phdrs[0]);
    e->ehdr.e_phnum = sizeof(e->phdrs) / sizeof(e->phdrs[0]);
    e->ehdr.e_shentsize = sizeof(e->shdrs[0]);
    e->ehdr.e_shnum = sizeof(e->shdrs) / sizeof(e->shdrs[0]);
    e->ehdr.e_shstrndx = 1;

    e->phdrs[0].p_type = PT_LOAD;
    e->phdrs[0].p_offset = 0;
    e->phdrs[0].p_vaddr = 0;
    e->phdrs[0].p_paddr = 0;
    e->phdrs[0].p_filesz = 0x1000;
    e->phdrs[0].p_memsz = 0x1000;
    e->phdrs[0].p_flags = PF_W | PF_R;
    e->phdrs[0].p_align = 0x1000;

    e->phdrs[1].p_type = PT_DYNAMIC;
    e->phdrs[1].p_offset = offsetof(m4_elf_linux_t, dyn);
    e->phdrs[1].p_vaddr = offsetof(m4_elf_linux_t, dyn);
    e->phdrs[1].p_paddr = offsetof(m4_elf_linux_t, dyn);
    e->phdrs[1].p_filesz = sizeof(e->dyn);
    e->phdrs[1].p_memsz = sizeof(e->dyn);
    e->phdrs[1].p_flags = PF_W | PF_R;
    e->phdrs[1].p_align = 4;

    e->phdrs[2].p_type = PT_LOAD;
    e->phdrs[2].p_offset = offsetof(m4_elf_linux_t, cont);
    e->phdrs[2].p_vaddr = 0x1000;
    e->phdrs[2].p_paddr = 0x1000;
    e->phdrs[2].p_filesz = bin_len;
    e->phdrs[2].p_memsz = bin_len;
    e->phdrs[2].p_flags = PF_X | PF_R;
    e->phdrs[2].p_align = 0x1000;

    e->shdrs[1].sh_name = 1;
    e->shdrs[1].sh_type = SHT_STRTAB;
    e->shdrs[1].sh_flags = 0;
    e->shdrs[1].sh_addr = offsetof(m4_elf_linux_t, shstrtab);
    e->shdrs[1].sh_offset = offsetof(m4_elf_linux_t, shstrtab);
    e->shdrs[1].sh_size = sizeof(e->shstrtab);
    e->shdrs[1].sh_link = 0;
    e->shdrs[1].sh_info = 0;
    e->shdrs[1].sh_addralign = 1;
    e->shdrs[1].sh_entsize = 0;

    e->shdrs[2].sh_name = 11;
    e->shdrs[2].sh_type = SHT_SYMTAB;
    e->shdrs[2].sh_flags = 0;
    e->shdrs[2].sh_addr = offsetof(m4_elf_linux_t, symtab);
    e->shdrs[2].sh_offset = offsetof(m4_elf_linux_t, symtab);
    e->shdrs[2].sh_size = sizeof(e->symtab);
    e->shdrs[2].sh_link = 3;
    e->shdrs[2].sh_info = 1;
    e->shdrs[2].sh_addralign = 4;
    e->shdrs[2].sh_entsize = sizeof(e->symtab[0]);

    e->shdrs[3].sh_name = 19;
    e->shdrs[3].sh_type = SHT_STRTAB;
    e->shdrs[3].sh_flags = 0;
    e->shdrs[3].sh_addr = offsetof(m4_elf_linux_t, strtab);
    e->shdrs[3].sh_offset = offsetof(m4_elf_linux_t, strtab);
    e->shdrs[3].sh_size = sizeof(e->strtab);
    e->shdrs[3].sh_link = 0;
    e->shdrs[3].sh_info = 0;
    e->shdrs[3].sh_addralign = 1;
    e->shdrs[3].sh_entsize = 0;

    e->shdrs[4].sh_name = 27;
    e->shdrs[4].sh_type = SHT_PROGBITS;
    e->shdrs[4].sh_flags = SHF_ALLOC | SHF_EXECINSTR;
    e->shdrs[4].sh_addr = 0x1000;
    e->shdrs[4].sh_offset = offsetof(m4_elf_linux_t, cont);
    e->shdrs[4].sh_size = bin_len;
    e->shdrs[4].sh_link = 0;
    e->shdrs[4].sh_info = 0;
    e->shdrs[4].sh_addralign = 4;
    e->shdrs[4].sh_entsize = 0;

    e->shdrs[5].sh_name = 33;
    e->shdrs[5].sh_type = SHT_DYNAMIC;
    e->shdrs[5].sh_flags = SHF_ALLOC | SHF_WRITE;
    e->shdrs[5].sh_addr = offsetof(m4_elf_linux_t, dyn);
    e->shdrs[5].sh_offset = offsetof(m4_elf_linux_t, dyn);
    e->shdrs[5].sh_size = sizeof(e->dyn);
    e->shdrs[5].sh_link = 6;
    e->shdrs[5].sh_info = 0;
    e->shdrs[5].sh_addralign = 4;
    e->shdrs[5].sh_entsize = sizeof(e->dyn[0]);

    e->shdrs[6].sh_name = 42;
    e->shdrs[6].sh_type = SHT_STRTAB;
    e->shdrs[6].sh_flags = SHF_ALLOC;
    e->shdrs[6].sh_addr = offsetof(m4_elf_linux_t, strtab);
    e->shdrs[6].sh_offset = offsetof(m4_elf_linux_t, strtab);
    e->shdrs[6].sh_size = sizeof(e->strtab);
    e->shdrs[6].sh_link = 0;
    e->shdrs[6].sh_info = 0;
    e->shdrs[6].sh_addralign = 1;
    e->shdrs[6].sh_entsize = 0;

    e->shdrs[7].sh_name = 50;
    e->shdrs[7].sh_type = SHT_DYNSYM;
    e->shdrs[7].sh_flags = SHF_ALLOC;
    e->shdrs[7].sh_addr = offsetof(m4_elf_linux_t, symtab);
    e->shdrs[7].sh_offset = offsetof(m4_elf_linux_t, symtab);
    e->shdrs[7].sh_size = sizeof(e->symtab);
    e->shdrs[7].sh_link = 6;
    e->shdrs[7].sh_info = 1;
    e->shdrs[7].sh_addralign = 4;
    e->shdrs[7].sh_entsize = sizeof(e->symtab[0]);

    e->shdrs[8].sh_name = 58;
    e->shdrs[8].sh_type = SHT_HASH;
    e->shdrs[8].sh_flags = SHF_ALLOC;
    e->shdrs[8].sh_addr = offsetof(m4_elf_linux_t, hash);
    e->shdrs[8].sh_offset = offsetof(m4_elf_linux_t, hash);
    e->shdrs[8].sh_size = sizeof(e->hash);
    e->shdrs[8].sh_link = 7;
    e->shdrs[8].sh_info = 0;
    e->shdrs[8].sh_addralign = 4;
    e->shdrs[8].sh_entsize = 4;

    e->dyn[0].d_tag = DT_STRTAB;
    e->dyn[0].d_un.d_val = offsetof(m4_elf_linux_t, strtab);

    e->dyn[1].d_tag = DT_SYMTAB;
    e->dyn[1].d_un.d_val = offsetof(m4_elf_linux_t, symtab);

    e->dyn[2].d_tag = DT_STRSZ;
    e->dyn[2].d_un.d_val = sizeof(e->strtab);

    e->dyn[3].d_tag = DT_SYMENT;
    e->dyn[3].d_un.d_val = sizeof(e->symtab[0]);

    e->dyn[4].d_tag = DT_HASH;
    e->dyn[4].d_un.d_val = offsetof(m4_elf_linux_t, hash);

    e->symtab[1].st_name = 1;
    e->symtab[1].st_value = 0x1000;
    e->symtab[1].st_size = bin_len;
    e->symtab[1].st_info = ELF32_ST_INFO(STB_GLOBAL, STT_FUNC);
    e->symtab[1].st_other = STV_DEFAULT;
    e->symtab[1].st_shndx = 4;

    e->hash.nbucket = 1;
    e->hash.nchain = 2;
    e->hash.bucket[0] = 1;
    e->hash.chain[0] = 0;
    e->hash.chain[1] = 0;

    memcpy(e->shstrtab, SHSTRTAB, sizeof(e->shstrtab));

    memcpy(e->strtab, STRTAB, sizeof(e->strtab));
}
