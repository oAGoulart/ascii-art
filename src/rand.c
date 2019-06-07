#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

/* platform specific stuff */
#if defined(_WIN32) || defined(_WIND64) || defined(__MINGW32__) || defined (__MINGW64__)
	#define WINDOWS

	/* WARNING: link Advapi32.lib on windows or this file won't compile */
	#include <wincrypt.h>

	#define MS_CONTAINER_NAME "oagoulart-rand-lotto"
#else
	#ifndef _XOPEN_SOURCE
	#define _XOPEN_SOURCE
	#endif
	#ifndef _XOPEN_SOURCE_EXTENDED
	#define _XOPEN_SOURCE_EXTENDED
	#endif

	#include <stdlib.h>
#endif

/* define games rules */
#define MEGA_TEN_MIN 6
#define MEGA_TEN_MAX 15
#define MEGA_NUM_MIN 1
#define MEGA_NUM_MAX 60
#define QUINA_TEN_MIN 5
#define QUINA_TEN_MAX 7
#define QUINA_NUM_MIN 1
#define QUINA_NUM_MAX 80

/* define data types */
#ifndef uint8_t
	typedef unsigned char uint8_t;
#endif

typedef enum option_e {
	OPTION_NONE = '\0', /* no option */
	OPTION_MEGA = 'M',  /* mega sena */
	OPTION_QUINA = 'Q', /* quina */
	OPTION_EXIT = 'E'   /* terminate */
} option_t;

/* wait given amout of miliseconds */
void wait(size_t milisec)
{
	clock_t init_clock = clock();

	/* loop for given time */
	while ((clock() - init_clock) / (CLOCKS_PER_SEC / 1000) < milisec);
}

/* generate random data and store to buffer */
bool generate_random_data(void* buffer, size_t size)
{
	uint8_t* address = buffer;
	bool error = true;

#ifdef WINDOWS
	HCRYPTPROV prov_handle;

	if (CryptAcquireContextA(&prov_handle, MS_CONTAINER_NAME, MS_DEF_PROV, PROV_RSA_FULL, CRYPT_SILENT)) {
		error = ~CryptGenRandom(prov_handle, size, address);
		CryptReleaseContext(prov_handle, 0);
	}
#else
	time_t timer = time(NULL);
	char* state = ctime(&timer);

	initstate(timer + clock(), state, strlen(state));

	for (int i = 0; i < size; i++)
		address[i] = (uint8_t)random();
#endif

	return ~error;
}

/* clear cli screem */
void clear_cli()
{
#ifdef WINDOWS
	system("cls");
#else
	system("clear && printf '\e[3J'");
#endif
}

/* change to upper case letter */
char to_upper(const char letter)
{
	return (letter >= 97 && letter <= 122) ? letter - 32 : letter;
}

/* draw menu and get user option */
option_t draw_menu()
{
	option_t option = OPTION_NONE;

	/* clear screen */
	clear_cli();

	/* draw menu interface */
	printf("\n=========================================================\n");
	printf("|          Human, welcome to the SkyNet Lotto!          |\n");
	printf("| It's a hundred percent trusty, bet here, win always!  |\n");
	printf("|                                                       |\n");
	printf("| To start, please choose an option:                    |\n");
	printf("|   %c - Mega Sena                                       |\n", OPTION_MEGA);
	printf("|   %c - Quina                                           |\n", OPTION_QUINA);
	printf("|   %c - Exit                                            |\n", OPTION_EXIT);
	printf("=========================================================\n");

	/* get user input */
	printf("Human, your choise: ");

	do {
		/* verify option was valid */
		if (option != OPTION_NONE && option != '\n') 
			printf("\nHuman, option '%c' is INVALID, please choose again: ", option);

		/* get option */
		option = to_upper(getchar());
	} while (option != OPTION_EXIT && option != OPTION_QUINA && option != OPTION_MEGA);

	return option;
}

/* generate game for human */
void generate_game(option_t game)
{
	if (game == OPTION_MEGA || game == OPTION_QUINA) {
		/* define constants */
		const int game_ten_min = (game == OPTION_MEGA) ? MEGA_TEN_MIN : QUINA_TEN_MIN;
		const int game_ten_max = (game == OPTION_MEGA) ? MEGA_TEN_MAX : QUINA_TEN_MAX;
		const int game_num_min = (game == OPTION_MEGA) ? MEGA_NUM_MIN : QUINA_NUM_MIN;
		const int game_num_max = (game == OPTION_MEGA) ? MEGA_NUM_MAX : QUINA_NUM_MAX;

		/* get number of bets */
		size_t num_bets = 0;

		do {
			printf("\nHuman, enter the number of bets: ");
			scanf("%u%*c", &num_bets);
		} while (num_bets < 1);

		/* get number of tens */
		size_t num_tens = 0;

		do {
			printf("\nHuman, enter the number of tens [min: %d max: %d]: ", game_ten_min, game_ten_max);
			scanf("%u%*c", &num_tens);
		} while (num_tens < game_ten_min || num_tens > game_ten_max);

		/* generate game */
		printf("... Processing arguments ... Connecting to SkyNet ...\n");
		wait(500); /* fake processing time :P */

		for (int i = 0; i < num_bets; i++) {
			uint8_t* buffer = NULL;

			/* verify memory was allocated */
			if ((buffer = malloc(num_tens)) != NULL) {
				printf("\nGame %d: ", i + 1);

				if (generate_random_data(buffer, num_tens)) {
					for (int j = 0; j < num_tens; j++)
						printf(" %u ", buffer[j] % game_num_max + game_num_min);
				}
				else {
					printf("error [0x534E] - coundn't connect to SkyNet!\nExtermination has been started automatically...");
					break;
				}

				/* free memory */
				free(buffer);
			}
			else {
				printf("error [0x736E] - SkyNet had a memory issue!\nExtermination has been started automatically...");
				break;
			}
		}

		/* go back to main loop */
		printf("\n - - - - - - - - - - - - - - - - - - - - - - - - - - -\n");
		printf("Human, type something to restart:\n");
		scanf("%*c%*c");
	}
}

/* draw option result */
void draw_option(option_t* option)
{
	printf("Human, you chose: %c ", *option);

	switch (*option) {
		case OPTION_MEGA:
			printf("- Mega Sena\n");
			generate_game(*option);
			break;

		case OPTION_QUINA:
			printf("- Quina\n");
			generate_game(*option);
			break;

		case OPTION_EXIT:
			printf("- EXTERMINATION\nHuman, there is too many humans, Human!\n");
			break;

		default:
			/* NOTE: this should not be run under any scenario */
			printf("- NOTHING\nHuman, something went incredibly wrong!\n");
			*option = OPTION_EXIT;
	}
}

/* entry point */
int main()
{
	option_t status = OPTION_NONE;

	/* main loop */
	while (status != OPTION_EXIT) {
		status = draw_menu();
		draw_option(&status);
	}

	return EXIT_SUCCESS;
}
