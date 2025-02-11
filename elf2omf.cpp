#include <algorithm>
#include <iterator>
#include <numeric>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <err.h>
#include <fcntl.h>
#include <sysexits.h>
#include <unistd.h>


#include "elf32.h"
#include "omf.h"

#ifdef __cpp_lib_endian
#include <bit>
using std::endian;
#else
#include "endian.h"
#endif

#include "bswap.h"

static_assert(sizeof(Elf32_Ehdr) == 0x34, "Invalid size for Elf32_Ehdr");
static_assert(sizeof(Elf32_Shdr) == 0x28, "Invalid size for Elf32_Shdr");
static_assert(sizeof(Elf32_Sym) == 16, "Invalid size for Elf32_Sym");
static_assert(sizeof(Elf32_Rel) == 8, "Invalid size for Elf32_Rel");
static_assert(sizeof(Elf32_Rela) == 12, "Invalid size for Elf32_Rela");

/*


 for now (small memory model - 1 segment)
 - merge all progbits
 - merge all bss

 - registers, tiny, ztiny merged together into direct page/stack segment

put bss/ds at the end (could use )


future
 - in omf, mark non-read-only segments as needing reload.

 */


struct {
	bool v = false;
	bool S = false;
	std::string o;

//	std::vector<std::string> l;
//	std::vector<std::string> L;

	unsigned stack = 0;
	unsigned errors = 0;
	uint16_t file_type = 0;
	uint32_t aux_type = 0;

	unsigned omf_flags = 0;
} flags;


struct reloc {
	unsigned src_segment = 0;
	unsigned src_offset = 0;
	unsigned dest_segment = 0;
	unsigned dest_offset = 0;

	unsigned symbol = 0;

	unsigned type = 0;
	unsigned addend = 0;
};

struct symbol {
	std::string name;
	int id = 0;

	uint8_t type = 0;
	uint8_t flags = 0;
	uint32_t offset = 0;
	int section = 0;
};


// this is our *merged* section, not an elf section
struct section {
	std::string name;
	int id = 0;

	uint8_t flags = 0;
	uint32_t size = 0;
	uint32_t align = 0;

	std::vector<uint8_t> data;
	std::vector<reloc> relocs;
};

std::unordered_map<std::string, int> global_section_map;
std::vector<section> global_sections;

std::unordered_map<std::string, int> global_symbol_map;
std::vector<symbol> global_symbols;

void throw_errno(const std::string &msg) {
	throw std::system_error(errno, std::generic_category(), msg); 
}

void throw_elf_error(const std::string &msg) {
	throw std::runtime_error(msg);
}

void throw_elf_error() {
	throw std::runtime_error("invalid elf file");
}


/* find or create a symbol */
symbol &find_symbol(const std::string &name) {
	auto iter = global_symbol_map.find(name);
	if (iter == global_symbol_map.end()) {
		global_symbols.push_back({});
		auto &s = global_symbols.back();
		s.name = name;
		s.id = global_symbols.size();

		global_symbol_map.emplace(name, s.id);
		return s;
	} else {
		return global_symbols[iter->second - 1];
	}
}

/* find or create a section */
section &find_section(const std::string &name) {

	auto iter = global_section_map.find(name);
	if (iter == global_section_map.end()) {
		global_sections.push_back({});
		auto &s = global_sections.back();
		s.name = name;
		s.id = global_sections.size();

		global_section_map.emplace(name, s.id);
		return s;
	} else {
		return global_sections[iter->second - 1];
	}
}

bool must_swap(const Elf32_Ehdr &header) {
	if (endian::native == endian::little) return header.e_ident[EI_DATA] == ELFDATA2MSB;
	if (endian::native == endian::big) return header.e_ident[EI_DATA] == ELFDATA2LSB;
	return false;
}

int load_elf_header(int fd, Elf32_Ehdr &header) {

	ssize_t ok = read(fd, &header, sizeof(header));
	if (ok < 0) throw_errno("read");
	if (ok != sizeof(header)) throw_elf_error();

	if (memcmp(header.e_ident, ELFMAG, SELFMAG)) throw_elf_error();

	if (must_swap(header)) bswap(header);

	return 0;
}

int load_elf_sections(int fd, const Elf32_Ehdr &header, std::vector<Elf32_Shdr> &sections) {


	sections.resize(header.e_shnum);

	if (lseek(fd, header.e_shoff, SEEK_SET) < 0) throw_errno("lseek");
	size_t ok = read(fd, sections.data(), header.e_shnum * sizeof(Elf32_Shdr));
	if (ok < 0) throw_errno("read");
	if (ok != header.e_shnum * sizeof(Elf32_Shdr)) throw_elf_error();

	if (must_swap(header)) {
		for (auto &e : sections) {
			bswap(e);
		}
	}

	return 0;
}

int load_elf_data(int fd, const Elf32_Ehdr &header, const Elf32_Shdr& section, std::vector<uint8_t> &rv) {

	if (lseek(fd, section.sh_offset, SEEK_SET) < 0)
		throw_errno("lseek");

	if (section.sh_size == 0) return 0; // just in case...

	size_t size = rv.size();
	rv.resize(size + section.sh_size);
	ssize_t ok = read(fd, rv.data() + size, section.sh_size);
	if (ok < 0) throw_errno("read");
	if (ok != section.sh_size) throw_elf_error();

	return 0;

}

int load_elf_string_table(int fd, const Elf32_Ehdr &header, const Elf32_Shdr& section, std::vector<uint8_t> &rv) {

	rv.resize(0);
	int ok = load_elf_data(fd, header, section, rv);
	if (ok == 0 && rv.size() && rv.back() != 0) rv.push_back(0);
	return ok;
}


template<class T>
int load_elf_type(int fd, const Elf32_Ehdr &header, const Elf32_Shdr& section, std::vector<T> &rv) {

	rv.resize(0);
	if (section.sh_size == 0) return 0;
	if (section.sh_entsize != sizeof(T)) throw_elf_error("unexpected entry size");

	if (lseek(fd, section.sh_offset, SEEK_SET) < 0)
		throw_errno("lseek");

	rv.resize(section.sh_size / section.sh_entsize);

	ssize_t ok = read(fd, rv.data(), section.sh_size);
	if (ok < 0) throw_errno("read");
	if (ok != section.sh_size) throw_elf_error();

	if (must_swap(header)) {
		for (auto &e : rv) {
			bswap(e);
		}
	}
	return 0;
}


bool is_dp(const std::string &name) {
	if (name.empty()) return false;
	char c = name.front();
	if (c == 'r' && name == "registers") return true;
	if (c == 'z' && name == "ztiny") return true;
	if (c == 't' && name == "tiny") return true;
	return false;
}


static unsigned type_to_size[16] = { 0, 1, 2, 3, 4, 8, 0, 0, 1, 2, 2, 1, 2, 3, 2, 2 };
static unsigned type_to_shift[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 16, 0, 16 };

/* do an absolute relocation. */
int abs_reloc(section &section, uint32_t offset, uint32_t value, unsigned type) {

	if (type > 15) return -1;
	unsigned size = type_to_size[type];
	unsigned shift = type_to_shift[type];

	if (offset >= section.data.size()) return -1;
	if (offset + size > section.data.size()) return -1;

	switch (type) {
	case 0:
	case 6:
	case 7:
		/* unknown type */
		return -1;
	case 1: /* dp */
	case 8:
		if (value > 0xff) warn(".tiny absolute relocation overflow");
		break;
	case 2: /* abs */
	case 9:
		if (value > 0xffff) warn(".near absolute relocation overflow");
		break;
	case 3: /* long */
		if (value > 0xffffff) warn(".far absolute relocation overflow");
		break;
	case 10: /* .kbank */
		if (value > 0xffff) warn(".kbank absolute relocation overflow");
		break;
	}

	if (shift) value <<= shift;
	for (unsigned i = 0; i < size; ++i, ++offset, value >>= 8) {
		section.data[offset] = value & 0xff;
	}

	return 0;
}

int one_file(const std::string &filename) {
	// process one elf file.

	// 1. open, verify it's a 65816 elf file
	// 2. merge sections
	// 3. update symbols
	// 4. process relocation records that refer to a private symbol????
	// 5. add relocation records ()

	// map local elf section to global section
	struct section_map {
		int section = 0;
		int offset = 0;
	};

	int fd;
	Elf32_Ehdr header;
	std::vector<Elf32_Shdr> sections;


	std::vector<section_map> local_section_map;

	fd = open(filename.c_str(), O_RDONLY);
	if (fd < 0) {
		warn("open %s", filename.c_str());
		return -1;
	}

	try {
		load_elf_header(fd, header);

		// verify elf file info
		bool ok = true;
		if (header.e_ident[EI_CLASS] != ELFCLASS32) ok = false;
		if (header.e_ident[EI_VERSION] != EV_CURRENT) ok = false;
		if (header.e_type != ET_REL) ok = false;
		if (header.e_machine != EM_65816) ok = false;
		if (header.e_version != EV_CURRENT) ok = false;
		if (header.e_shentsize != sizeof(Elf32_Shdr)) ok = false;
		if (header.e_shnum == 0) ok = false;

		if (!ok) {
			throw_elf_error("Not a 65816 elf file");
		}


		load_elf_sections(fd, header, sections);

		std::vector<uint8_t> string_table;
		load_elf_string_table(fd, header, sections[header.e_shstrndx], string_table);


		local_section_map.resize(header.e_shnum + 1);

		// pass 1 - process SHT_PROGBITS and SHT_NOBITS
		int sh_num = -1;
		for (const auto &s : sections) {

			++sh_num;
			if (s.sh_type != SHT_PROGBITS && s.sh_type != SHT_NOBITS) continue;

			// if (s.sh_name == 0) continue;

			std::string name;
			if (s.sh_name && s.sh_name < string_table.size())
				name = (char *)string_table.data() + s.sh_name;


			section &gs = find_section(name);

			// todo flags, etc.

			if (gs.align < s.sh_addralign) {
				gs.align = s.sh_addralign;
			}

			auto &data = gs.data;
			if (gs.align > 1) {
				unsigned mask = (gs.align - 1);
				if (data.size() & mask)
					data.resize((data.size() + mask) & ~mask);
			}

			local_section_map[sh_num].section = gs.id;
			local_section_map[sh_num].offset = data.size();

			if (s.sh_type == SHT_PROGBITS) {
				load_elf_data(fd, header, s, data);
			} else {
				data.resize(data.size() + s.sh_size);
			}
		}

		// pass 2 - process relocation records and symbol table records.
		// i suppose there could be multiple symbol table sections but there's most likely just one...

		int current_st = -1;
		sh_num = -1;
		std::vector<Elf32_Sym> st;

		for (const auto &s : sections) {
			++sh_num;
			if (s.sh_type == SHT_REL || s.sh_type == SHT_RELA) {

				if (current_st != s.sh_link) {
					if (s.sh_link == 0 || s.sh_link >= sections.size()) throw_elf_error("bad sh_link");
					load_elf_type<Elf32_Sym>(fd, header, sections[s.sh_link], st);
					current_st = s.sh_link;
				}

				unsigned ix = local_section_map[s.sh_link].section;
				unsigned offset = local_section_map[s.sh_link].offset;
				auto &gs = global_sections[ix];

				std::vector<Elf32_Rela> rels;

				if (s.sh_type == SHT_REL) {
					std::vector<Elf32_Rel> tmp;
					load_elf_type<Elf32_Rel>(fd, header, s, tmp);
					std::transform(tmp.begin(), tmp.end(), std::back_inserter(rels), [](const Elf32_Rel &r){
						Elf32_Rela rr = { r.r_offset, r.r_info, 0 };
						return rr;
					});
				} else {
					load_elf_type<Elf32_Rela>(fd, header, s, rels);
				}

				for (const auto &r : rels) {
					int rsym = ELF32_R_SYM(r.r_info);
					int rtype = ELF32_R_TYPE(r.r_info);

					const auto &sym = st[rsym];
					// type -> size/shift info
					unsigned bind = ELF32_ST_BIND(sym.st_info);
					unsigned type = ELF32_R_TYPE(sym.st_info);

					reloc rr;

					rr.dest_segment = local_section_map[s.sh_link].section;
					rr.dest_offset = local_section_map[s.sh_link].offset + r.r_offset;

					rr.type = rtype;
					rr.addend = r.r_addend;

					// todo -- verify address is valid?

					if (sym.st_shndx == SHN_ABS) {
						// this is a constant number (should never happen) so relocate now....

						abs_reloc(gs, rr.dest_offset, sym.st_value, rtype);
						continue;
					}

					std::string name = (char *)string_table.data() + sym.st_name;

					if (sym.st_shndx == SHN_UNDEF ) {
						auto &gs = find_symbol(name); // possibly generate a new symbol table entry.
						rr.symbol = gs.id;
						rr.src_segment = 0; 
						rr.src_offset = 0; 
					} else {
						// local symbol!
						rr.src_segment = local_section_map[sym.st_shndx].offset;
						rr.src_offset = local_section_map[sym.st_shndx].section;
					}
					gs.relocs.push_back(rr);
				}
				continue;
			}
			if (s.sh_type == SHT_SYMTAB) {

				if (current_st != sh_num) {
					load_elf_type<Elf32_Sym>(fd, header, s, st);
					current_st = sh_num;
				}

				for (const auto &x : st) {
					// copy global/weak symbols to the global symbol table
					// skip local symbols.
					unsigned bind = ELF32_ST_BIND(x.st_info);
					unsigned type = ELF32_ST_BIND(x.st_info);

					if (bind == STB_LOCAL) continue;
					if (x.st_shndx == 0) continue; // undefined -- just a reference.

					if (x.st_name == 0 || x.st_name >= string_table.size()) continue;
					std::string name = (char *)string_table.data() + x.st_name;

					symbol &gs = find_symbol(name);
					if (x.st_shndx == 0) continue; // just a reference, so nothing to update.

					if (gs.section == 0) {
						// new symbol!  let's define it
						if (x.st_shndx == SHN_ABS) {
							gs.offset = x.st_value;
							gs.section = -1;
						} else {
							gs.section = local_section_map[x.st_shndx].section;
							gs.offset = x.st_value + local_section_map[x.st_shndx].offset;
						}
					 } else {
					 	// known symbol.  weak is ok, otherwise, warn.
						if (bind == STB_GLOBAL) {
							// allow duplicate absolute symbols?
							warnx("%s: duplicate symbol (%s)", filename.c_str(), name.c_str());
							continue;
						}
					}
				}
			}
		}
	} catch(std::exception &ex) {
		close(fd);
		warnx("%s: %s", filename.c_str(), ex.what());
		return -1;
	}

	close(fd);
	return 0;
}



bool parse_ft(const std::string &s) {

	// gcc doesn't like std::xdigit w/ std::all_of

	if (s.length() != 2 && s.length() != 7) return false;
	if (!std::all_of(s.begin(), s.begin() + 2, isxdigit)) return false;
	if (s.length() == 7) {
		if (s[2] != ',' && s[2] != ':') return false;
		if (!std::all_of(s.begin() + 3, s.end(), isxdigit)) return false;
	}

	auto lambda = [](int lhs, uint8_t rhs){
		lhs <<= 4;
		if (rhs <= '9') return lhs + rhs - '0';
		return lhs + (rhs | 0x20) - 'a' + 10;
	};

	flags.file_type = std::accumulate(s.begin(), s.begin() + 2, 0, lambda);
	flags.aux_type = 0;

	if (s.length() == 7)
		flags.aux_type = std::accumulate(s.begin() + 3, s.end(), 0, lambda);

	return true;
}

void usage(int ec = EX_USAGE) {
	fputs("usage elf2omf [flags] file...\n"
		"Flags:\n"
			" -h               show usage\n"
			" -v               be verbose\n"
			" -X               inhibit ExpressLoad segment\n"
			" -C               inhibit SUPER records\n"
			" -S size          specify stack segment size (default 4096)\n"
			" -1               generate version 1 OMF File\n"
			" -o file          specify outfile name\n"
//			" -l library       specify library\n"
//			" -L path          specify library path\n"
			" -t xx[:xxxx]     specify file type\n"
		, stderr);
	exit(ec);
}



int main(int argc, char **argv) {

	int ch;
	std::string outfile;

	while ((ch = getopt(argc, argv, "o:vh")) != -1) {
		switch (ch) {
			case 'o': flags.o = optarg; break;
			case 'v': flags.v = true; break;

			case '1': flags.omf_flags |= OMF_V1; break;
			case 'C': flags.omf_flags |= OMF_NO_SUPER; break;
			case 'S': flags.S = true; break;
			case 'X': flags.omf_flags |= OMF_NO_EXPRESS; break;

			case 'h': usage(0);

			case 't': {
				// -t xx[:xxxx] -- set file/auxtype.
				if (!parse_ft(optarg)) {
					errx(EX_USAGE, "Invalid -t argument: %s", optarg);
				}
				break;
			}

			default: usage();
		}
	}

	argv += optind;
	argc -= optind;

	if (!argc) usage();


	if (flags.o.empty()) flags.o = "out.omf";

	for (int i = 0; i < argc; ++i) {
		std::string name = argv[i];
		if (one_file(name) < 0) ++flags.errors;
	}

	// if there are any missing symbols, search library files...

	// merge sections into omf segments...
	// (and build the stack segment...)
	// do any absolute relocations....
	// convert relocations to omf relocations
	// save the omf file.
	// done!


	exit(0);
}