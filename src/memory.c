#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* platform specific stuff */
#if defined(_WIN32) || defined(_WIND64) || defined(__MINGW32__) || defined (__MINGW64__)
	#define WINDOWS

	/* NOTE: link Kernel32.lib and Ws2_32.lib on windows */
	#ifndef WIN32_LEAN_AND_MEAN
 	#define WIN32_LEAN_AND_MEAN
 	#endif
	#include <Windows.h>
	#include <Memoryapi.h>
	#include <winsock2.h>

	/* default memory protection modes */
	#define MEM_UNPROT PAGE_EXECUTE_READWRITE
	#define MEM_PROT PAGE_EXECUTE_READ
#else /* assume POSIX */
	#include <unistd.h>
	#include <sys/select.h>
	#include <sys/mman.h>

	/* default memory protection modes */
	#define MEM_UNPROT PROT_READ | PROT_WRITE | PROT_EXEC
	#define MEM_PROT PROT_READ | PROT_EXEC
#endif

#if defined(__ILP32__) || defined(_ILP32) || defined(__i386__) || defined(_M_IX86) || defined(_X86_)
	#define X86

	typedef uint32_t ulong_t;
#else /* assume 64-bit */
	typedef uint64_t ulong_t;
#endif

typedef unsigned char ubyte_t;

/* asm cmds */
const uint8_t JMP_OPCODE = 233;

/* set memory protection */
static void set_memory_protection(const void* address, const size_t size, const int mode) {
	if (address != NULL && size > 0) {
#ifdef WINDOWS
		VirtualProtect((LPVOID)address, size, mode, &old_mode);
#else
		/* get size of pages */
		long page_sz = sysconf(_SC_PAGESIZE);

		/* find page pointer */
		void* page_ptr = (long*)((long)address & ~(page_sz - 1));

		/* no previous mode to be returned */
		mprotect(page_ptr, size, mode);
#endif
	}
}

static int memory_get_protection(void* address) {
	int result = 0;

	if (address != NULL) {
#ifdef WINDOWS
		MEMORY_BASIC_INFORMATION page_info;
		if (VirtualQuery(address, &page_info, sizeof(MEMORY_BASIC_INFORMATION)) == sizeof(MEMORY_BASIC_INFORMATION))
			result = page_info.Protect;
#else
		FILE* maps = fopen("/proc/self/maps", "r"); /* currently mapped memory regions */
		uintptr_t block[2] = { 0, 0 };              /* block of memory addresses */
		char perms[5];                              /* set of permissions */

		/* parse file lines */
		while (fscanf(maps, "%x-%x %4s %*x %*d:%*d %*d %*s\n", &block[0], &block[1], perms) == 3) {
			if (block[0] <= (uintptr_t)address && block[1] >= (uintptr_t)address) {
				result |= (perms[0] == 'r') ? PROT_READ : PROT_NONE;  /* can be readed */
				result |= (perms[1] == 'w') ? PROT_WRITE : PROT_NONE; /* can be written */
				result |= (perms[2] == 'x') ? PROT_EXEC : PROT_NONE;  /* can be executed */
				break;
			}
		}
#endif
	}

	return result;
}

/* write data into memory */
static void set_raw(void* address, const void* data, const size_t size, const bool vp)
{
	if (address != NULL && data != NULL && size > 0) {
		if (vp) {
			/* store memory protection for later */
			int old_mode = memory_get_protection(address);

			/* remove virtual protection */
			set_memory_protection(address, size, MEM_UNPROT);

			/* write data into memory */
			memcpy(address, data, size);

			/* reset virtual protection */
			set_memory_protection(address, size, old_mode);
		}
		else
			memcpy(address, data, size);
	}
}

/* set jump into address */
static void set_jump(void* address, const void* dest, const bool vp)
{
	if (address != NULL && dest != NULL) {
		/* find destination offset */
		ulong_t offset = (ulong_t)dest - ((ulong_t)address + 1 + sizeof(ulong_t));

		/* write opcode */
		set_raw(address, &JMP_OPCODE, 1, true);

		/* write destination offset */
		set_raw((void*)((ulong_t)address + 1), &offset, sizeof(ulong_t), true);
	}
}

/* get char from stdin without blocking */
static int get_char()
{
	char result = EOF;

	/* define select() values */
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(0, &readfds);

	struct timeval timeout = { 0, 0 };

	/* verify stdin is not empty */
	if (select(1, &readfds, NULL, NULL, &timeout))
		result = getc(stdin);

	return result;
}

int main()
{
	ubyte_t* addr = (void*)&getchar;

	if (getchar())
		set_jump(&getchar, &get_char, true);

	printf("%x %x %x %x %x\n%x\n", *addr, *(addr + 1), *(addr + 2), *(addr + 3), *(addr + 4), *(int*)&getchar);

	while (1) {
		printf("Let's hook this baby! %c\n", getchar());
	}
}
