#include <ctype.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define GREY "\x1B[90;40m"
#define YELLOW "\x1B[33;40m"
#define GREEN "\x1B[94;40m"
#define RED "\x1B[31;40m"
#define RESET "\x1B[0m"
#define CLEAR_LINE "\x1B[K"
#define LEFT(i) "\x1B[" i "D"
#define UP(i) "\x1B[" i "A"

#define GREYBG "\x1B[37;41m"
#define YELLOWBG "\x1B[37;43m"
#define GREENBG "\x1B[37;104m"

extern const char *wordle_words[];
extern int number_of_words;

int is_real_word(char *word) {
    for (int i = 0; i < number_of_words; i++) {
        if (strcmp(word, wordle_words[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

unsigned long tried = 0;
unsigned long missed = 0;
unsigned long hit = 0;
int needs_unkeyboard = 0;
const char *real_word;

const char *keyboard = "qwertyuiop\n asdfghjkl\n  zxcvbnm";

void render_keyboard(void) {
    printf(CLEAR_LINE);
    char c = keyboard[0];
    for (int i = 1; c; c = keyboard[i++]) {
        if (c == '\n') {
            needs_unkeyboard += 1;
            printf("\n ");
            continue;
        }
        if (c == ' ') {
            printf(" ");
            continue;
        }
        int x = c - 'a';
        if ((hit >> x) & 1) {
            printf(GREENBG "%c" RESET " ", c);
        } else if ((missed >> x) & 1) {
            printf(YELLOWBG "%c" RESET " ", c);
        } else if ((tried >> x) & 1) {
            printf(GREYBG "%c" RESET " ", c);
        } else {
            printf("%c ", c);
        }
    }
}

int wordle_read(char word[static 6]) {
    int line_width = 0;
    bool printed_space = true;
    bool cleared = false;
    char c;

    printf(" ");
    while ((c = getchar()) != EOF) {
        cleared = false;
        for (int i = 0; i < needs_unkeyboard; i++) {
            printf("\r" CLEAR_LINE UP("1") " " CLEAR_LINE);
            cleared = true;
        }
        needs_unkeyboard = 0;
        if (c >= 'A' && c <= 'Z') {
            c += 'a' - 'A';
        }
        if (isspace(c) && c != '\n') {
            continue;
        } else if (c == '?' && line_width == 0) {
            render_keyboard();
        } else if (c == '#' && line_width == 0) {
            printf("Give up so soon? Okay, it was %s\r\n ", real_word);
        } else if (isprint(c)) {
            if (line_width == 5) continue;
            printf(CLEAR_LINE "%c ", c);
            word[line_width] = c;
            line_width += 1;
        } else if (c == '\n') {
            if (line_width == 5) {
                if (is_real_word(word))  return 1;
                if (cleared)  continue;
                char *message = "  not a real word";
                printf(RED "%s" RESET LEFT("%li"), message, strlen(message));
                continue;
            } else {
                char *message = "  more input needed (5 letters)";
                if (cleared)  continue;
                printf(RED "%s" RESET LEFT("%li"), message, strlen(message));
                continue;
            }
            line_width = 0;
        } else if (c == 0x7f) {
            if (line_width == 0) continue;
            printf("\b\b" CLEAR_LINE);
            line_width -= 1;
        }
        if (line_width == 0 && !printed_space) {
            printf(" ");
        }
    }
}

// return schema:
// value >> index*2 & 3:
//   0: not in word
//   1: wrong position
//   2: exact match
unsigned wordle_check(const char *real, const char *guess) {
    unsigned result = 0;

    for (int i = 0; i < 5; i++) {
        tried |= 1 << (guess[i] - 'a');
        if (real[i] == guess[i]) {
            result |= 2 << (i*2);
            hit |= 1 << (guess[i] - 'a');
        } else if (memchr(real, guess[i], 5)) {
            result |= 1 << (i*2);
            missed |= 1 << (guess[i] - 'a');
        }
    }

    return result;
}

#define RESULT_WIN 0x2aa

const char *result_colors[] = {
    [0] = GREY,
    [1] = YELLOW,
    [2] = GREEN,
};

void render_result(char *word, unsigned result) {
    printf("\r" CLEAR_LINE " ");
    for (int i = 0; i < 5; i++) {
        printf("%s%c ", result_colors[(result >> (i*2)) & 3], word[i]);
    }
    printf(RESET "\n");
}

void wordle_round(const char *real_word) {
    char buf[6];
    int n_tries = 0;
    while (true) {
        wordle_read(buf);
        unsigned result = wordle_check(real_word, buf);
        render_result(buf, result);
        n_tries += 1;
        if (result == RESULT_WIN) {
            printf(GREEN "\r\n You Win! (%i tries)\r\n" RESET, n_tries);
            break;
        }
    }
    tried = 0;
    hit = 0;
    missed = 0;
    needs_unkeyboard = 0;
}

struct termios termios_orig;

void sigint_handler(int signal) {
    tcsetattr(fileno(stdin), 0, &termios_orig);
    exit(0);
}

int main() {
    struct termios termios;
    char c;

    tcgetattr(fileno(stdin), &termios);
    memcpy(&termios_orig, &termios, sizeof(termios));
    termios.c_lflag &= ~(ECHO | ECHONL | ICANON); // | IEXTEN);

    signal(SIGINT, sigint_handler);

    tcsetattr(fileno(stdin), 0, &termios);
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    int word_id;

    srand(time(NULL));

    while (true) {
        word_id = rand() % number_of_words;
        const char *word = wordle_words[word_id];
        real_word = word;
        wordle_round(word);
    }
}
