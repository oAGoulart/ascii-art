#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/mman.h>

#define UBYTE uint8_t
#define JMP_OPCODE 0xE9

/* get char from stdin without blocking */
static int get_char()
{
	char result = EOF;

	/* define select() values */
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(0, &readfds);

	struct timeval timeout = { 0 , 0 };

	/* verify stdin is not empty */
	if (select(1, &readfds, NULL, NULL, &timeout))
		result = getc(stdin);

	return result;
}

/* set memory virtual protection */
static bool virtual_protect(void* address, size_t size, bool vp) {
  long page_size = sysconf(_SC_PAGESIZE);

  /* find page pointer */
  void* page_ptr = (void*)((long)address & ~(page_size - 1));

  if (!vp)
    return (mprotect(page_ptr, size, PROT_READ | PROT_WRITE | PROT_EXEC) == 0);
  else
    return (mprotect(page_ptr, size, PROT_READ | PROT_EXEC) == 0);
}

/* set jump into address */
static void set_jump(void* address, void* destination)
{
	/* try to remove virtual protection */
	if (virtual_protect(address, 1 + sizeof(void*), false)) {
		UBYTE* byte_addr = address;

		/* find destination offset */
		uint32_t offset = (uint32_t)(destination - (address + 5));

		/* put jump opcode */
		*byte_addr = JMP_OPCODE;

		/* put offset after opcode */
		memcpy(byte_addr + 1, &offset, sizeof(uint32_t));

		/* reset virtual protection */
		virtual_protect(address, 1 + sizeof(void*), true);
	}
}

int main()
{
	UBYTE* addr = (void*)&getchar;

	if (getchar())
		set_jump(&getchar, &get_char);

	printf("%x %x %x %x %x\n%x\n", *addr, *(addr + 1), *(addr + 2), *(addr + 3), *(addr + 4), (unsigned int)&getchar);

	while (1) {
		if (getchar() > 0) {
			printf("Let's hook this baby!\n");
		}
	}
}
