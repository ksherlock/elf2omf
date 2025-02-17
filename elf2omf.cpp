#include <algorithm>
#include <iterator>
#include <numeric>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
	unsigned offset = 0;
	unsigned value = 0;

	int symbol = 0;
	unsigned type = 0;
};



struct symbol {
	std::string name;
	int id = 0;

	// uint8_t type = 0;
	// uint8_t flags = 0;
	uint32_t offset = 0;
	int section = 0;
	unsigned count = 0; // number of references
	bool local = false;
	bool absolute = false;
};

enum {
	TYPE_CODE = 1,
	TYPE_DATA,
	TYPE_CDATA,
	TYPE_BSS
};

enum {
	REGION_DP = 1,
	REGION_NEAR,
	REGION_FAR,
	REGION_HUGE,
};

// this is our *merged* section, not an elf section
struct section {
	std::string name;
	int id = 0;

	uint32_t align = 0;

	unsigned type = 0;
	unsigned region = 0;


	unsigned bss_size = 0;

	std::vector<uint8_t> data;
	std::vector<reloc> relocs;
	// std::vector<unsigned> symbols;

	unsigned omf_segment = 0;
	unsigned omf_offset = 0;


	unsigned size() const {
		return type == TYPE_BSS ? bss_size : data.size();
	}

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

unsigned name_to_region(const std::string &name) {
	static std::unordered_map<std::string, unsigned> map = {
		{"registers", REGION_DP},
		{"tiny", REGION_DP},
		{"ztiny", REGION_DP},
		{"stack", REGION_DP},

		{"data", REGION_NEAR},
		{"cdata", REGION_NEAR},
		{"zdata", REGION_NEAR},

		{"near", REGION_NEAR},
		{"cnear", REGION_NEAR},
		{"znear", REGION_NEAR},

		{"far", REGION_FAR},
		{"cfar", REGION_FAR},
		{"zfar", REGION_FAR},

		{"huge", REGION_FAR},
		{"chuge", REGION_FAR},
		{"zhuge", REGION_FAR},
	};


	auto iter = map.find(name);
	return (iter == map.end()) ? 0 : iter->second; 
}

unsigned type_to_type(unsigned sh_type, unsigned sh_flags) {
	if (sh_type == SHT_NOBITS) return TYPE_BSS;

	if (sh_type == SHT_PROGBITS) {
		if (sh_flags & SHF_EXECINSTR) return TYPE_CODE;
		if (sh_flags & SHF_WRITE) return TYPE_DATA;
		return TYPE_CDATA;
	}
	// should not happen...
	return 0;
}

typedef std::unordered_map<std::string, unsigned> symbol_map;
typedef std::unordered_map<std::string, unsigned> section_map;

/* find or create a symbol */
symbol &find_symbol(const std::string &name) {
	auto iter = global_symbol_map.find(name);
	if (iter == global_symbol_map.end()) {
		auto &sym = global_symbols.emplace_back();
		sym.name = name;
		sym.id = global_symbols.size();

		global_symbol_map.emplace(name, sym.id);
		return sym;
	}
	return global_symbols[iter->second - 1];
}

symbol *maybe_find_symbol(const std::string &name) {
	auto iter = global_symbol_map.find(name);
	return (iter == global_symbol_map.end()) ? nullptr : &global_symbols[iter->second - 1];
}


#if 0
symbol &find_symbol(const std::string &name, bool anonymous) {

	auto iter = global_symbol_map.find(name);
	if (iter == global_symbol_map.end()) {
		auto &sym = global_symbols.emplace_back();
		sym.name = name;
		sym.id = global_symbols.size();

		if (!anonymous) global_symbol_map.emplace(name, sym.id);
		return sym;
	}
	return global_symbols[iter->second - 1];
}
#endif

// lookup/create a symbol table first in the local table, then in the global table.
symbol &find_symbol(const std::string &name, std::unordered_map<std::string, int> &local_symbol_map, bool create_locally = false) {

	auto iter = local_symbol_map.find(name);
	if (iter == local_symbol_map.end()) {
		if (!create_locally) return find_symbol(name);
		auto &sym = global_symbols.emplace_back();
		sym.name = name;
		sym.id = global_symbols.size();
		local_symbol_map.emplace(name, sym.id);
		return sym;
	}

	return global_symbols[iter->second - 1];
}



/* find or create a section */
section &find_section(const std::string &name) {

	/* special case for known dp segments! */

	auto iter = global_section_map.find(name);
	if (iter == global_section_map.end()) {
		auto &s = global_sections.emplace_back();
		s.name = name;
		s.id = global_sections.size();


		s.region = name_to_region(name);

		global_section_map.emplace(name, s.id);
		return s;
	}
	return global_sections[iter->second - 1];
}

section *maybe_find_section(const std::string &name) {
	auto iter = global_section_map.find(name);
	if (iter == global_section_map.end()) return nullptr;
	return &global_sections[iter->second - 1];
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





static unsigned type_to_size[16] = { 0, 1, 2, 3, 4, 8, 0, 0, 1, 2, 2, 1, 2, 3, 2, 2 };
static unsigned type_to_shift[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 16, 0, 16 };

int abs_reloc(std::vector<uint8_t> &data, uint32_t offset, uint32_t value, unsigned type) {

	if (type > 15) return -1;
	unsigned size = type_to_size[type];
	unsigned shift = type_to_shift[type];

	if (offset >= data.size()) return -1;
	if (offset + size > data.size()) return -1;

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

	if (shift) value >>= shift;
	for (unsigned i = 0; i < size; ++i, ++offset, value >>= 8) {
		data[offset] = value & 0xff;
	}

	return 0;
}

/* do an absolute relocation. */
int abs_reloc(section &section, uint32_t offset, uint32_t value, unsigned type) {

	return abs_reloc(section.data, offset, value, type);
}



void simplify_relocations() {
	// 1. reify SHN_ABS
	// 2. reify dp references, if possible.
	// n.b. - section #1 is the merged dp/stack section.

	for (auto &s : global_sections) {

		auto it = std::remove_if(s.relocs.begin(), s.relocs.end(), [&](const auto &r){

			const auto &sym = global_symbols[r.symbol - 1];
			if (sym.section == -1) {
				// absolute
				abs_reloc(s, r.offset, sym.offset, r.type);
				return true;
			}

#if 0
			if (sym.section == 1) {
				if (r.type == 1 || r.type == 8 || r.type == 11) {
					abs_reloc(s, r.offset, r.value + sym.offset, r.type);
					return true;
				}
			}
#endif
			return false;
		});
		s.relocs.erase(it, s.relocs.end());
	}
}

// .section attributes:
// rodata -> SHT_PROGBITS, SHF_ALLOC
// text   -> SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR
// data   -> SHT_PROGBITS, SHF_ALLOC | SHF_WRITE
// bss    -> SHT_NOBITS,   SHF_ALLOC | SHF_WRITE


// linker generated symbols:
// _O\x03.sectionStart_[name]
// _O\x03.sectionEnd_[name]
// _O\x03.sectionSize_[name] (32-bit absolute)
// _DirectPageStart
 
/*
 * standard segments
 * dp:
 *   registers, tiny, ztiny
 * code:
 *   code, compactcode,farcode,
 * data:
 *   data, cdata (ro) zdata (bss)
 *   near, cnear (ro), znear (bss)
 *   far , cfar (ro), zfar (bss)
 *   huge, chuge, (ro) zhuge (bss)
 *
 * calypsi linker-generated:
 *   itiny, idata, inear, ifar, ihuge, data_init_table
 * other:
 *   reset, stack, heap
 *
 */



// if there is a stack segment, it will be adjusted later
void generate_linker_symbols(void) {
	// .sectionStart, .sectionEnd, .sectionSize.
	for (auto &s : global_sections) {

		std::string name;
		symbol *sym;

		name = "_O\x03.sectionStart_" + s.name;
		if ((sym = maybe_find_symbol(name))) {
			sym->section = s.id;
			sym->offset = 0;
		}

		name = "_O\x03.sectionEnd_" + s.name;
		if ((sym = maybe_find_symbol(name))) {
			sym->section = s.id;
			sym->offset = s.data.size() - 1;
		}

		name = "_O\x03.sectionSize_" + s.name;
		if ((sym = maybe_find_symbol(name))) {
			sym->section = -1;
			sym->offset = s.data.size();
			sym->absolute = true;
		}
	}
}


bool check_for_missing_symbols(bool pass1 = true) {

	// linker-generated symbols.
	static std::unordered_set<std::string> skippable = {
		"_DirectPageStart", "_NearBaseAddress",
		"_O\x03.sectionStart_stack",
		"_O\x03.sectionEnd_stack",
		"_O\x03.sectionSize_stack",
	};

	bool ok = true;
	for (auto &sym : global_symbols) {

		if (!sym.count) continue;
		if (sym.absolute) continue;
		if (sym.section == 0) {
			if (pass1 && skippable.count(sym.name)) continue;
			warnx("undefined symbol: %s", sym.name.c_str());
			ok = false;
		}
	}
	return ok;
}


template<class T>
void move_or_append(std::vector<T> &out, std::vector<T> &in) {

	if (out.empty()) {
		out = std::move(in);
	} else {
		out.insert(out.end(), in.begin(), in.end());
	}
}


template<class T>
void append(std::vector<T> &out, const std::vector<T> &in) {

	out.insert(out.end(), in.begin(), in.end());
}

typedef std::vector<std::reference_wrapper<section>> section_ref_vector ;

// counts up region sizes.
unsigned analyze(const section_ref_vector &sections, unsigned data[5][7]) {

	unsigned total = 0;
	for (const section &s : sections) {

		unsigned sz = data[s.region][s.type];
		unsigned x = s.size();

		unsigned mask = 0;
		if (s.align > 1) {
			mask = s.align - 1;
			sz = (sz + mask) & ~mask;
			total = (total + mask) & ~mask;
		}

		data[s.region][s.type] = sz + x;
		total += x;

		// total this region excl code [?]
		if (s.type != TYPE_CODE) {
			sz = data[s.region][5];
			if (mask) sz = (sz + mask) & ~mask;
			data[s.region][5] = sz + x;
		}

		// total this region
		sz = data[s.region][6];
		if (mask) sz = (sz + mask) & ~mask;
		data[s.region][6] = sz + x;
	}
	return total;
}


void to_omf(void) {


	section *stack = nullptr;
	// make sure there's a stack segment if needed.
	{
		auto s = maybe_find_section("stack");
		if (s) {
			stack = s;
			if (s->type != TYPE_BSS) errx(1, "stack section not bss");
			s->bss_size = (s->bss_size + 0xff) & ~0xff;
			s->align = 0;
			if (flags.stack && flags.stack != s->size()) {
				warnx("-S ignored");
			}
		} else if (flags.stack) {
			auto &s = find_section("stack");
			s.type = TYPE_BSS;
			s.bss_size = flags.stack;
			stack = &s;
		}
	}


	section_ref_vector sections;
	section_ref_vector dp_sections;

	sections.reserve(global_sections.size());
	dp_sections.reserve(4);

	for (auto &s : global_sections) {
		if (s.type == TYPE_BSS) continue;

		if (s.region == REGION_DP) {
			dp_sections.push_back(std::ref(s));
		} else {
			sections.push_back(std::ref(s));
		}
	}
	// bss last
	for (auto &s : global_sections) {
		if (s.type != TYPE_BSS) continue;

		if (s.region == REGION_DP) {
			if (&s == stack) continue; // special handling... 
			dp_sections.push_back(std::ref(s));
		} else {
			sections.push_back(std::ref(s));
		}
	}

	// sort the dp sections - registers, tiny, ztiny, stack
	// n.b. - aside from stack, these are alphabetical. so just compare the first letter.
	std::sort(dp_sections.begin(), dp_sections.end(), [](const section &a, const section &b){
		unsigned ca = a.name.front();
		unsigned cb = b.name.front();
		if (ca == 's') ca = 'z'+1;
		if (cb == 's') cb = 'z'+1;
		return ca < cb;
	});


	unsigned sizes[5][7] = {};

	unsigned total = analyze(sections, sizes);
	analyze(dp_sections, sizes);
	// best case -- 1 segment!

	// link types
	// 1. everything goes in 1 segment
	// 2. all near segments merged
	// 3. all near data merged, separate near code
	// 4. 


	std::vector<omf::segment> segments;
	unsigned segnum = 1;
	if (total < 0x010000) {
		auto &seg = segments.emplace_back();
		seg.segnum = segnum++;


		unsigned offset = 0;
		bool need_align = false;
		for (section &s : sections) {

			unsigned mask = 0;
			unsigned sz = s.size();
			if (s.align > 1) {
				need_align = true;
				mask = s.align - 1;
			}



			if (s.type == TYPE_BSS) {

				if (mask) {
					unsigned orig = offset;
					offset = (offset + mask) & ~mask;
					seg.reserved_space += (offset - orig);
				}
				seg.reserved_space += sz;
			} else {

				if (mask) {
					offset = (offset + mask) & ~mask;
					seg.data.resize(offset);
				}

				append(seg.data, s.data);
			}

			s.omf_segment = seg.segnum;
			s.omf_offset = offset;
			offset += sz;
		}
		// omf only has page or bank alignment.
		if (need_align) seg.alignment = 0x0100;


		//
		symbol *sym;
		if ((sym = maybe_find_symbol("_NearBaseAddress"))) {
			const section &s = sections.front(); 
			sym->section = s.id;
			sym->offset = 0;
		}

	} else {
		errx(1, "not yet...");
	}

	// now handle the dp segment.
	unsigned dp_size = analyze(dp_sections, sizes);

	// could also check if we need an explicit direct page --
	// via symbols (.sectionStart stack, _DirectPageStart)

	if (dp_size > 0 || stack) {

		if (dp_size > 0xff) {
			errx(1, "dp exceeds 1 page ($%04x)", dp_size);
		}

		if (dp_size > 0 && !stack) {
			errx(1, "stack section missing.  Use -S size or create a bss stack section.");
		}

		if (stack && stack->size() > 0x8000) {
			errx(1, "stack too big ($%04x)", stack->size());
		}

		int stack_size = stack ? stack->size() - dp_size : 0;
		if (stack && stack_size <= 0)
			errx(1, "stack too small ($%04x)", stack_size);

		auto &seg = segments.emplace_back();
		seg.segnum = segnum++;
		seg.kind = 0x12; // dp/stack
		seg.alignment = 0x0100; // redundant
		seg.segname = "dp/stack";

		if (stack) {
			stack->align = 0; // no alignment
			stack->bss_size = stack_size;
			dp_sections.emplace_back(std::ref(*stack));

			symbol *sym;
			// update symbols (only size and end shoud be changing)

			if ((sym = maybe_find_symbol("_O\x03.sectionStart_stack"))) {
				sym->section = stack->id;
				sym->offset = 0;
			}
			if ((sym = maybe_find_symbol("_O\x03.sectionEnd_stack"))) {
				sym->section = stack->id;
				sym->offset = stack_size - 1;
			}
			if ((sym = maybe_find_symbol("_O\x03.sectionSize_stack"))) {
				sym->section = -1;
				sym->offset = stack_size;
				sym->absolute = true;
			}
		}

		if (!dp_sections.empty()) {
			symbol *sym;
			section &s = dp_sections.front();
			if ((sym = maybe_find_symbol("_DirectPageStart"))) {
				sym->section = s.id;
				sym->offset = 0;
			}
		}


		unsigned offset = 0;
		for (section &s : dp_sections) {

			unsigned mask = 0;
			unsigned sz = s.size();
			if (s.align > 1) {
				mask = s.align - 1;
			}

			if (s.type == TYPE_BSS) {

				if (mask) {
					unsigned orig = offset;
					offset = (offset + mask) & ~mask;
					seg.reserved_space += (offset - orig);
				}
				seg.reserved_space += sz;
			} else {

				if (mask) {
					offset = (offset + mask) & ~mask;
					seg.data.resize(offset);
				}
				append(seg.data, s.data);
			}

			s.omf_segment = seg.segnum;
			s.omf_offset = offset;
			offset += sz;
		}

	}

	// ok, now we can convert dp relocations into absolute values.
	// ... and convert relocations to omf relocations.


	append(sections, dp_sections);

	for (const section &s : sections) {

		auto &seg = segments[s.omf_segment - 1];

		for (const auto &r : s.relocs) {

			const auto &sym = global_symbols[r.symbol - 1];

			unsigned offset = r.offset + s.omf_offset;
			unsigned value = r.value + sym.offset;

			if (sym.absolute) {
				abs_reloc(seg.data, offset, value, r.type);
				continue;
			}

			if (!r.symbol) errx(1, "relocation missing symbol");

			const auto &src = global_sections[sym.section - 1];

			value += src.omf_offset;

			// convert dp reference to a constant.
			if (r.type == 1 || r.type == 8 || r.type == 11) {
				abs_reloc(seg.data, offset, value, r.type);
				continue;
			}

			unsigned size = type_to_size[r.type];
			unsigned shift = -type_to_shift[r.type];

			if (src.omf_segment == s.omf_segment) {
				omf::reloc rr;
				rr.size = size;
				rr.shift = shift;
				rr.offset = offset;
				rr.value = value;
				seg.relocs.push_back(rr);
			} else {
				omf::interseg is;
				is.size = size;
				is.shift = shift;
				is.offset = offset;
				is.segment = src.omf_segment;
				is.segment_offset = value;
				seg.intersegs.push_back(is);
			}

		}

	}


	save_omf(flags.o, segments, flags.omf_flags);
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

	if (flags.v) printf("%s...\n", filename.c_str());
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
		// also load the symbol table.
		// i suppose there could be > 1 symbol table but only one supported for now.
		int current_st = -1;
		std::vector<Elf32_Sym> st;

		int sh_num = -1;
		for (const auto &s : sections) {

			++sh_num;
			if (s.sh_type == SHT_SYMTAB) {
				if (current_st != -1) throw_elf_error("multiple symbol tables");
				current_st = sh_num;
				load_elf_type<Elf32_Sym>(fd, header, s, st);
				continue;
			}

			std::string name;
			if (s.sh_name && s.sh_name < string_table.size())
				name = (char *)string_table.data() + s.sh_name;


			if (s.sh_type == SHT_NOBITS) {


				section &gs = find_section(name);
				if (gs.type == 0) gs.type = TYPE_BSS;


				if (gs.type != TYPE_BSS)
					errx(1, "%s: %s - section type mismatch", filename.c_str(), name.c_str());


				gs.align = std::max(gs.align, s.sh_addralign);
				if (gs.align > 1) {
					unsigned mask = gs.align - 1;
					gs.bss_size = (gs.bss_size + mask) & ~mask;
				}				

				local_section_map[sh_num].section = gs.id;
				local_section_map[sh_num].offset = gs.bss_size;

				gs.bss_size += s.sh_size;
				continue;
			}

			if (s.sh_type == SHT_PROGBITS) {

				section &gs = find_section(name);
				unsigned t = type_to_type(s.sh_type, s.sh_flags);
				if (gs.type == 0) {
					gs.type = t;
				}
				if (gs.type != t) {
					errx(1, "%s:%s - section type mismatch", filename.c_str(), name.c_str());
				}

				auto &data = gs.data;
				gs.align = std::max(gs.align, s.sh_addralign);

				if (gs.align > 1) {
					unsigned mask = (gs.align - 1);
					if (data.size() & mask)
						data.resize((data.size() + mask) & ~mask);
				}

				local_section_map[sh_num].section = gs.id;
				local_section_map[sh_num].offset = data.size();

				load_elf_data(fd, header, s, data);
			}

		}



		// pass 1.5 -- load the symbol tables.
		// local symbols go into the global symbol table but not the global symbol table map.
		std::unordered_map<std::string, int> local_symbol_map;

		std::vector<int> symbol_to_symbol;
		symbol_to_symbol.reserve(st.size());


		for (const auto &x : st) {

			// copy global/weak symbols to the global symbol table
			// skip local symbols.
			unsigned bind = ELF32_ST_BIND(x.st_info);
			unsigned type = ELF32_ST_BIND(x.st_info);


			if (x.st_name == 0 || x.st_name >= string_table.size()) {
				symbol_to_symbol.push_back(0);
				continue;
			}
			std::string name = (char *)string_table.data() + x.st_name;


			// todo -- the .calypsi_info section can make some undefined references
			// a .require-ment. (also noreorder, others?)


			symbol &sym = find_symbol(name, local_symbol_map, bind == STB_LOCAL);

			symbol_to_symbol.push_back(sym.id);

			bool required = false; // todo....
			if (required) sym.count++;

			if (x.st_shndx == 0) continue;

#if 0
			if (x.st_shndx != SHN_UNDEF && x.st_shndx != SHN_ABS) {
				auto &s = global_sections[local_sections [x.st_shndx] - 1];
				s.symbols.push_back(sym.id);
			}
#endif

			if (sym.section == 0) {
				// new symbol!  let's define it
				// todo -- SHN_COMMON?

				if (bind == STB_LOCAL) sym.local = true;

				if (x.st_shndx == SHN_COMMON) {
					errx(1, "SHN_COMMON not yet supported.");
				}
				if (x.st_shndx == SHN_ABS) {
					sym.absolute = true;
					sym.offset = x.st_value;
					sym.section = -1;
				} else {
					// SHN_UNDEF handled above.
					sym.section = local_section_map[x.st_shndx].section;
					sym.offset = x.st_value + local_section_map[x.st_shndx].offset;
				}

			 } else {

				if (bind == STB_LOCAL) {
					warnx("%s: duplicate symbol (%s)", filename.c_str(), name.c_str());
					continue;
				}

			 	// known symbol.  weak is ok, otherwise, warn.
				if (bind == STB_GLOBAL) {
					// allow duplicate absolute symbols?
					warnx("%s: duplicate symbol (%s)", filename.c_str(), name.c_str());
					continue;
				}
			}
		}

		// build map of elf symbol table entries to global_symbol entries?


		// pass 2 - process relocation records.


		sh_num = -1;

		// pass 2 - process the relocation records.
		for (const auto &s : sections) {
			++sh_num;
			if (s.sh_type != SHT_REL && s.sh_type != SHT_RELA) continue;


			// sh_link is the associated symbol table.
			// sh_info is the associated data section 

			if (current_st != s.sh_link) {
				throw_elf_error("bad symbol table reference");
			}

			unsigned section_offset = local_section_map[s.sh_info].offset;
			unsigned section_id = local_section_map[s.sh_info].section;

			section &gs = global_sections[section_id - 1];

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

				if (rsym > symbol_to_symbol.size() || !symbol_to_symbol[rsym]) {
					warnx("%s: bad relocation", filename.c_str());
					continue;
				}


				// const auto &sym = st[rsym];

				// unsigned bind = ELF32_ST_BIND(sym.st_info);
				// unsigned type = ELF32_R_TYPE(sym.st_info);

				auto &sym = global_symbols[symbol_to_symbol[rsym] - 1];
				sym.count++;

				reloc rr;

				rr.offset = section_offset + r.r_offset;

				rr.type = rtype;
				rr.value = r.r_addend;
				rr.symbol = sym.id;
				gs.relocs.push_back(rr);
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

void init(void) {

// idea... create empty placeholder register, tiny, ztiny, stack segments
// so they're in the order I want them?

#if 0
	// create dp/stack segment for 
	// this does not go in the name map.
	{
		auto &s = global_sections.emplace_back();
		s.name = "dp/stack";
		s.id = 1;
	}

	{
		auto &sym = find_symbol("_DirectPageStart");
		sym.section = 1;
	}
	// todo -- "_NearBaseAddress" --> near data bank. 
#endif
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

bool parse_stack(const std::string &s) {
	if (s.empty()) return false;

	int rv = 0;
	size_t end = 0;
	try {
		rv = std::stoi(s, &end, 10);
	} catch (std::exception &ex) {
		return false;
	}
	if (rv < 0) return false;
	rv = (rv + 0xff) & ~0xff;
	if (rv > 0x8000) return false; // in theory 48k
	if (end != s.length()) return false;
	flags.stack = rv;
	return true;
}

void usage(int ec = EX_USAGE) {
	fputs("usage elf2omf [flags] file...\n"
		"Flags:\n"
			" -h               show usage\n"
			" -v               be verbose\n"
			" -X               inhibit ExpressLoad segment\n"
			" -C               inhibit SUPER records\n"
			" -S size          specify stack segment size\n"
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

	while ((ch = getopt(argc, argv, "ht:o:v1CS:X")) != -1) {
		switch (ch) {
			case 'o': flags.o = optarg; break;
			case 'v': flags.v = true; break;

			case '1': flags.omf_flags |= OMF_V1; break;
			case 'C': flags.omf_flags |= OMF_NO_SUPER; break;
			case 'X': flags.omf_flags |= OMF_NO_EXPRESS; break;

			case 'h': usage(0);

			case 't': {
				// -t xx[:xxxx] -- set file/auxtype.
				if (!parse_ft(optarg)) {
					errx(EX_USAGE, "Invalid -t argument: %s", optarg);
				}
				break;
			}

			case 'S': {
				if (!parse_stack(optarg)) {
					errx(EX_USAGE, "Invalid -S argument: %s", optarg);
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


	init();


	for (int i = 0; i < argc; ++i) {
		std::string name = argv[i];
		if (one_file(name) < 0) ++flags.errors;
	}


	// debug - dump sections
	if (flags.v) {
		printf("Sections:\n");
		for (const auto &s : global_sections) {
			printf("% 3d %-16s %ld\n", s.id, s.name.c_str(), s.data.size());
		}
		printf("Symbols:\n");
		for (const auto &s : global_symbols) {
			char m = ' ';
			if (s.section == 0) m = '?';
			else if (s.section == -1) m = '#'; // abs
			printf("% 3d %c %-16s\n", s.id, m, s.name.c_str());
		}
	}

	// if there are any missing symbols, search library files...

	generate_linker_symbols();
	if (!check_for_missing_symbols()) exit(1);
	to_omf();


	// merge sections into omf segments...
	// (and build the stack segment...)
	// do any absolute relocations....
	// convert relocations to omf relocations
	// save the omf file.
	// done!


	exit(0);
}