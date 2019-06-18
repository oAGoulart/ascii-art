#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* platform specific stuff */
#if defined(_WIN32) || defined(_WIND64) || defined(__MINGW32__) || defined (__MINGW64__)
	#define WINDOWS

	/* WARNING: link Kernel32.lib and Ws2_32.lib on windows */
	#ifndef _WIN32_WINNT
 	#define _WIN32_WINNT 0x0501
 	#endif
	#include <Windows.h>
	#include <Memoryapi.h>
	#include <winsock2.h>

	/* default memory protection modes */
	#define MEM_NO_PROT PAGE_EXECUTE_READWRITE
	#define MEM_PROT PAGE_EXECUTE_READ
#else /* assume POSIX */
	#include <unistd.h>
	#include <sys/select.h>
	#include <sys/mman.h>

	/* default memory protection modes */
	#define MEM_NO_PROT PROT_READ | PROT_WRITE | PROT_EXEC
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

/* set memory virtual protection */
static int virtual_protect(void* address, const size_t size, const int mode) {
	int old_mode = -1;

	if (address != NULL && size > 0) {
#ifdef WINDOWS
		VirtualProtect((LPVOID)address, size, mode, &old_mode);
#else
		/* get size of pages */
		long page_sz = sysconf(_SC_PAGESIZE);

		/* find page pointer */
		void* page_ptr = (long*)((long)address & ~(page_sz - 1));

		/* use the macro definition of protected memory */
		old_mode = (mprotect(page_ptr, size, mode) == 0) ? MEM_PROT : -2;
#endif
	}

	return old_mode;
}

/* write assembly command into memory */
static void set_raw(void* address, const void* data, const size_t size, const bool vp)
{
	if (address != NULL && data != NULL && size > 0) {
		if (vp) {
			/* store previous memory protection mode */
			int old_mode = MEM_PROT;

			/* try to remove virtual protection */
			if ((old_mode = virtual_protect(address, size, MEM_NO_PROT)) ) {
				/* write data into memory */
				memcpy(address, data, size);

				/* reset virtual protection */
				virtual_protect(address, size, old_mode);
			}
		}
		else
			memcpy(address, data, size);
	}
}

/* set jump into address */
static void set_jump(void* address, void* dest, const bool vp)
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
		if (getchar() == EOF)
			printf("Let's hook this baby!\n");
	}
}
