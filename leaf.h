/**
 * Leaf - a single header ELF loader.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <elf.h>
#include <errno.h>
#include <dlfcn.h>

#ifdef LEAF_32BIT
#define LeafEhdr Elf32_Ehdr
#define LeafPhdr Elf32_Phdr
#define LeafDyn  Elf32_Dyn
#define LeafRela Elf32_Rela
#define LeafSym  Elf32_Sym
#define LEAF_CURRENT_CLASS 1
#else
#define LeafEhdr Elf64_Ehdr
#define LeafPhdr Elf64_Phdr
#define LeafDyn  Elf64_Dyn
#define LeafRela Elf64_Rela
#define LeafSym  Elf64_Sym
#define LEAF_CURRENT_CLASS 2
#endif

typedef struct Leaf {
	LeafEhdr *ehdr;
	LeafPhdr **phdrs;
	void *blob;
	void **dl_handles;
	size_t dl_handle_count;
} Leaf;

typedef struct LeafStream {
	uint8_t *data;
	size_t size;
	size_t pos;
} LeafStream;

#ifdef LEAF_IMPLEMENTATION

static LeafStream *LeafStreamInit(uint8_t *buffer, size_t size) {
	/**
	 * Makes a read stream around the given buffer
	 */
	
	LeafStream *self = malloc(sizeof *self);
	
	if (!self) {
		return NULL;
	}
	
	memset(self, 0, sizeof *self);
	
	self->data = buffer;
	self->size = size;
	
	return self;
}

static size_t LeafStreamReadInto(LeafStream *self, size_t count, void *buffer) {
	/**
	 * Read data from the stream into the given buffer, returns number of bytes
	 * read.
	 */
	
	if (self->pos + count > self->size) {
		return 0;
	}
	
	memcpy(buffer, self->data + self->pos, count);
	
	self->pos += count;
	
	return count;
}

static void *LeafStreamRead(LeafStream *self, size_t count) {
	if (self->pos + count > self->size) {
		return NULL;
	}
	
	void *data = malloc(count);
	
	if (!data) {
		return NULL;
	}
	
	LeafStreamReadInto(self, count, data);
	
	return data;
}

static size_t LeafStreamGetpos(LeafStream *self) {
	return self->pos;
}

static void *LeafStreamGetptr(LeafStream *self) {
	return self->data + self->pos;
}

static void LeafStreamSetpos(LeafStream *self, size_t pos) {
	self->pos = pos;
}

static void LeafStreamFree(LeafStream *self) {
	free(self);
}

Leaf *LeafInit(void) {
	/**
	 * Initialise a new instance of Leaf with the given parameters.
	 */
	
	Leaf *self = malloc(sizeof *self);
	
	if (!self) {
		return NULL;
	}
	
	memset(self, 0, sizeof *self);
	
	return self;
}

void *LeafMakeMap(size_t size) {
	return mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

uint8_t ELF_SIGNATURE[] = {0x7f, 'E', 'L', 'F'};

const char *LeafLoadFromBuffer(Leaf *self, void *contents, size_t length) {
	/**
	 * Returns a string containing details of the error that occured, or NULL
	 * on success
	 */
	
	// Init a read stream
	LeafStream *stream = LeafStreamInit(contents, length);
	
	// Read header
	self->ehdr = LeafStreamRead(stream, sizeof *self->ehdr);
	
	if (!self->ehdr) {
		return "Failed to read header";
	}
	
	if (memcmp(self->ehdr, ELF_SIGNATURE, 4)) {
		return "Invalid ELF file";
	}
	
	if (self->ehdr->e_ident[EI_CLASS] != LEAF_CURRENT_CLASS) {
		return "Incorrect binary class for this platform";
	}
	
	if (self->ehdr->e_ident[EI_DATA] != 1) {
		return "Big endian is not supported";
	}
	
	if (self->ehdr->e_ident[EI_VERSION] != 1) {
		return "Too new or invalid ELF version";
	}
	
	if (self->ehdr->e_type != ET_DYN) {
		return "Only loading shared objects is supported";
	}
	
	// TODO: e_machine
	
	// Program and section headers
	size_t phoff = self->ehdr->e_phoff;
	size_t phentsize = self->ehdr->e_phentsize;
	size_t phnum = self->ehdr->e_phnum;
	
	// Read program headers
	// https://www.sco.com/developers/gabi/2003-12-17/ch5.pheader.html
	self->phdrs = malloc((phnum + 1) * sizeof *self->phdrs);
	
	if (!self->phdrs) {
		return "Failed to alloc phdrs array";
	}
	
	self->phdrs[phnum] = NULL;
	
	LeafStreamSetpos(stream, phoff);
	
	for (size_t i = 0; i < phnum; i++) {
		LeafPhdr *phdr = LeafStreamRead(stream, phentsize);
		
		if (!phdr) {
			return "Failed to read a program header";
		}
		
		self->phdrs[i] = phdr;
	}
	
	// Determine how much memory we need to map based on loadable segment sizes
	// since base address == 0 for ET_DYN we can just use the highest VirtAddr
	// + MemSiz value
	size_t highest = 0;
	
	for (size_t i = 0; i < phnum; i++) {
		if (self->phdrs[i]->p_type == PT_LOAD) {
			// don't need to check if its larger, PT_LOAD's should be sorted
			// by p_vaddr from low->high
			highest = self->phdrs[i]->p_vaddr + self->phdrs[i]->p_memsz;
		}
	}
	
	printf("leaf: highest value = 0x%zx, mapping...\n", highest);
	
	// Map memory for loadable segments, copy their contents
	self->blob = LeafMakeMap(highest);
	
	if (self->blob == MAP_FAILED) {
		return strerror(errno);
	}
	
	// load code, data, etc and also find location of dynamic symbol table
	printf("leaf: mapped at <%p>, copying...\n", self->blob);
	
	LeafDyn *dyns = NULL;
	
	for (size_t i = 0; i < phnum; i++) {
		LeafPhdr *phdr = self->phdrs[i];
		
		if (phdr->p_type == PT_LOAD) {
			LeafStreamSetpos(stream, phdr->p_offset);
			LeafStreamReadInto(stream, phdr->p_filesz, self->blob + phdr->p_vaddr);
		}
		else if (phdr->p_type == PT_DYNAMIC) {
			LeafStreamSetpos(stream, phdr->p_offset);
			dyns = LeafStreamGetptr(stream);
		}
		// I think we can ignore the PT_GNU_STACK and PT_GNU_RELRO, but maybe
		// not PT_GNU_EH_FRAME ?
	}
	
	if (!dyns) {
		return "Failed to find dynamic info";
	}
	
	// Get information from dynamic segment
	// WARNING: Lots of unimplemented stuff here, only implemented what's from
	// libsmashhit.so
	// NOTE: try only to use things from the loaded blob now
	const char *strtab = NULL;
	size_t strtab_size;
	
	LeafRela *relocs = NULL;
	size_t reloc_size;
	size_t reloc_ent_size;
	
	LeafSym *symtab = NULL;
	size_t sym_count = 0;
	size_t sym_ent_size;
	
	void *init_array = NULL;
	size_t init_array_size;
	
	void *fini_array = NULL;
	size_t fini_array_size;
	
	for (size_t i = 0; dyns[i].d_tag != DT_NULL; i++) {
		switch (dyns[i].d_tag) {
			case DT_NEEDED: {
				printf("Leaf: DT_NEEDED 0x%zx\n", dyns[i].d_un.d_val);
				// TODO: check if it fails
				self->dl_handles = realloc(self->dl_handles, (self->dl_handle_count + 1) * sizeof *self->dl_handles);
				self->dl_handles[self->dl_handle_count] = (void *) dyns[i].d_un.d_ptr; // we will fix the pointers later
				self->dl_handle_count += 1;
				break;
			}
			case DT_HASH: {
				sym_count = ((Elf32_Word *)(self->blob + dyns[i].d_un.d_ptr))[1];
				break;
			}
			case DT_STRTAB: {
				strtab = self->blob + dyns[i].d_un.d_ptr;
				break;
			}
			case DT_SYMTAB: {
				symtab = self->blob + dyns[i].d_un.d_ptr;
				break;
			}
			case DT_RELA: {
				relocs = self->blob + dyns[i].d_un.d_ptr;
				break;
			}
			case DT_RELASZ: {
				reloc_size = dyns[i].d_un.d_val;
				break;
			}
			case DT_RELAENT: {
				reloc_ent_size = dyns[i].d_un.d_val;
				break;
			}
			case DT_STRSZ: {
				strtab_size = dyns[i].d_un.d_val;
				break;
			}
			case DT_SYMENT: {
				sym_ent_size = dyns[i].d_un.d_val;
				break;
			}
			case DT_SYMBOLIC: {
				printf("Leaf: DT_SYMBOLIC\n");
				break;
			}
			case DT_BIND_NOW: {
				printf("Leaf: DT_BIND_NOW\n");
				break;
			}
			case DT_INIT_ARRAY: {
				init_array = self->blob + dyns[i].d_un.d_ptr;
				break;
			}
			case DT_FINI_ARRAY: {
				fini_array = self->blob + dyns[i].d_un.d_ptr;
				break;
			}
			case DT_INIT_ARRAYSZ: {
				init_array_size = dyns[i].d_un.d_val;
				break;
			}
			case DT_FINI_ARRAYSZ: {
				fini_array_size = dyns[i].d_un.d_val;
				break;
			}
			default: {
				printf("Unknown dynamic section entry: 0x%zx\n", dyns[i].d_tag);
				break;
			}
		}
	}
	
	if (!strtab) { return "Could not find string table address"; }
	if (!relocs) { return "Could not find relocs"; }
	if (!symtab) { return "Could not find symbol table address"; }
	if (!init_array) { return "Could not find init array address"; }
	if (!fini_array) { return "Could not find fini array address"; }
	if (!sym_count) { return "Could not find number of symbols"; }
	
	// Correct needed library string names
	for (size_t i = 0; i < self->dl_handle_count; i++) {
		self->dl_handles[i] += (size_t)strtab;
	}
	
	// Load dependent libraries
	for (size_t i = 0; i < self->dl_handle_count; i++) {
		printf("Dep lib soname: %s\n", (char *)self->dl_handles[i]);
		self->dl_handles[i] = dlopen(self->dl_handles[i], RTLD_NOW | RTLD_GLOBAL);
		if (!self->dl_handles[i]) {
			printf("Loading lib failed! Continuing anyways...\n");
		}
	}
	
	// Build symbol table
	// TODO
	printf("Have %zd symbols\n", sym_count);
	
	// Preform relocations
	// TODO
	size_t reloc_count = reloc_size / reloc_ent_size;
	printf("Will preform %zu relocations...\n", reloc_count);
	
	// Call init functions
	// TODO
	
	LeafStreamFree(stream); // TODO free if it fails
	
	return NULL;
}

const char *LeafLoadFromFile(Leaf *self, const char *path) {
	FILE *file = fopen(path, "rb");
	
	if (!file) {
		return "Could not open file";
	}
	
	fseek(file, 0, SEEK_END);
	long length = ftell(file);
	fseek(file, 0, SEEK_SET);
	
	uint8_t *data = malloc(length);
	
	if (!data) {
		fclose(file);
		return "Failed to allocate data";
	}
	
	if (fread(data, 1, length, file) != length) {
		fclose(file); free(data);
		return "Failed to read data";
	}
	
	fclose(file);
	
	const char *error = LeafLoadFromBuffer(self, data, length);
	
	free(data);
	
	return error;
}

void LeafFree(Leaf *self) {
	free(self);
	
	return;
}

#endif
