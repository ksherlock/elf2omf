
#if 0
uint8_t byteswap(uint8_t value) {
	return value;
}

uint16_t byteswap(uint16_t value) {
	return __builtin_bswap16(value);
}

int16_t byteswap(int16_t value) {
	return __builtin_bswap16(value);
}

uint32_t byteswap(uint32_t value) {
	return __builtin_bswap32(value);
}

int32_t byteswap(int32_t value) {
	return __builtin_bswap32(value);
}

uint64_t byteswap(uint64_t value) {
	return __builtin_bswap64(value);
}

int64_t byteswap(int64_t value) {
	return __builtin_bswap64(value);
}
#endif

inline void bswap(int8_t) { }
inline void bswap(uint8_t) { }
inline void bswap(int16_t x) { x = __builtin_bswap16(x); }
inline void bswap(uint16_t x) { x = __builtin_bswap16(x); }
inline void bswap(int32_t x) { x = __builtin_bswap32(x); }
inline void bswap(uint32_t x) { x = __builtin_bswap32(x); }
inline void bswap(int64_t x) { x = __builtin_bswap64(x); }
inline void bswap(uint64_t x) { x = __builtin_bswap64(x); }



void bswap(Elf32_Ehdr &x) {
	bswap(x.e_type);
	bswap(x.e_machine);
	bswap(x.e_version);
	bswap(x.e_entry);
	bswap(x.e_phoff);
	bswap(x.e_shoff);
	bswap(x.e_flags);
	bswap(x.e_ehsize);
	bswap(x.e_phentsize);
	bswap(x.e_phnum);
	bswap(x.e_shentsize);
	bswap(x.e_shnum);
	bswap(x.e_shstrndx);
}

void bswap(Elf32_Shdr &x) {
	bswap(x.sh_name);
	bswap(x.sh_type);
	bswap(x.sh_flags);
	bswap(x.sh_addr);
	bswap(x.sh_offset);
	bswap(x.sh_size);
	bswap(x.sh_link);
	bswap(x.sh_info);
	bswap(x.sh_addralign);
	bswap(x.sh_entsize);	
}

void bswap(Elf32_Sym &x) {
	bswap(x.st_name);
	bswap(x.st_value);
	bswap(x.st_size);
	bswap(x.st_info);
	bswap(x.st_other);
	bswap(x.st_shndx);
}

void bswap(Elf32_Rel &x) {
	bswap(x.r_offset);
	bswap(x.r_info);
}

void bswap(Elf32_Rela &x) {
	bswap(x.r_offset);
	bswap(x.r_info);
	bswap(x.r_addend);
}
