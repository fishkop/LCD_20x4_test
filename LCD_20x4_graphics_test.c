/* =========================================================================
 * LCD_20x4_graphics_test.c
 *
 * Zweck
 * -----
 * Ergaenzung zu LCD_20x4_test.c: testet die *Grafikfaehigkeiten* eines
 * HD44780-kompatiblen Zeichen-LCD mit PCF8574-I2C-Backpack:
 *
 *   1) CGRAM-Demo: definiert 8 eigene, frei waehlbare 5x8-Pixel-Symbole
 *      (Herz, Smiley, Pfeil, Haken, Kreuz, Glocke, Note, Gradzeichen) und
 *      zeigt sie an. CGRAM (Character Generator RAM) ist ein Standard-
 *      feature JEDES HD44780-kompatiblen Controllers, funktioniert also
 *      unabhaengig vom Hersteller/Modul.
 *   2) ROM-Sweep: zeigt der Reihe nach die Codes 0x80-0xFF (erweiterter,
 *      im Controller-ROM fest hinterlegter Zeichensatz). Was dort genau
 *      erscheint, ist ROM-Varianten-abhaengig (haeufig "A00": Katakana +
 *      Sonderzeichen, oder "A02": westeuropaeische Sonderzeichen) - dieser
 *      Sweep macht sichtbar, was das jeweils angeschlossene Modul bietet.
 *
 * Kompilieren
 * -----------
 *   gcc -O2 -Wall -o LCD_20x4_graphics_test LCD_20x4_graphics_test.c
 *   (gleiche Flags/Begruendung wie in LCD_20x4_test.c: -O2 Optimierung,
 *   -Wall alle Warnungen, keine externen Bibliotheken noetig.)
 *
 * Start-Optionen (Kommandozeile)
 * -------------------------------
 *   ./LCD_20x4_graphics_test [Optionen]
 *
 *   -b PFAD   I2C-Bus-Geraetedatei (Standard: /dev/i2c-1)
 *   -a HEX    I2C-Adresse des Displays in Hex (Standard: 27)
 *   -r N      Anzahl Zeilen des Displays (Standard: 4)
 *   -c N      Anzahl Spalten des Displays (Standard: 20)
 *   -1        Nur einmal komplett durchlaufen, dann beenden
 *   -h        Kurzhilfe anzeigen und beenden
 *
 * Endlosschleife starten/stoppen
 * -------------------------------
 * Vordergrund (mit Strg+C beenden, Display wird dabei geloescht):
 *   ./LCD_20x4_graphics_test
 *
 * Hintergrund (ueber SSH-Sitzung hinweg laufen lassen):
 *   nohup ./LCD_20x4_graphics_test >lcd_graphics_test.log 2>&1 &
 *   echo $!                    # PID merken
 *
 * Stoppen:
 *   kill <PID>                 # sauberes Beenden via SIGTERM-Handler
 *   pkill -f LCD_20x4_graphics_test   # falls PID nicht mehr bekannt
 *
 * Eigene CGRAM-Symbole fuer andere Projekte anpassen
 * ----------------------------------------------------
 * Die 8 Symbol-Bitmaps stehen weiter unten als einfache 8-Byte-Arrays
 * (CHAR_HEART, CHAR_SMILEY, ...). Jedes Byte ist eine Pixelzeile des
 * 5x8-Zeichens, die unteren 5 Bit zaehlen (Bit4..Bit0 = Pixelspalte
 * links..rechts). Eigene Symbole lassen sich z. B. mit einem der vielen
 * Online-"HD44780 Custom Character Generator"-Tools entwerfen und hier
 * als weiteres 8-Byte-Array eintragen (maximal 8 Symbole gleichzeitig,
 * Indizes 0-7, da der Controller nur 8 CGRAM-Slots hat).
 *
 * ========================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

static char g_bus_path[256] = "/dev/i2c-1";
static int  g_i2c_addr      = 0x27;
static int  g_rows          = 4;
static int  g_cols          = 20;
static int  g_once          = 0;

#define LCD_BACKLIGHT 0x08
#define ENABLE_BIT    0x04
#define RS_BIT        0x01

static const int row_offsets_20col[4] = {0x00, 0x40, 0x14, 0x54};
static int i2c_fd = -1;
static volatile sig_atomic_t stop_requested = 0;

static void i2c_write_byte(unsigned char data) {
    if (write(i2c_fd, &data, 1) != 1) perror("i2c write");
}

static void lcd_pulse_enable(unsigned char data) {
    i2c_write_byte(data | ENABLE_BIT);
    usleep(600);
    i2c_write_byte(data & (unsigned char)~ENABLE_BIT);
    usleep(600);
}

static void lcd_write4bits(unsigned char nibble, unsigned char rs) {
    unsigned char data = (nibble & 0xF0) | LCD_BACKLIGHT | (rs ? RS_BIT : 0);
    i2c_write_byte(data);
    lcd_pulse_enable(data);
}

static void lcd_send(unsigned char value, unsigned char rs) {
    lcd_write4bits(value & 0xF0, rs);
    lcd_write4bits((unsigned char)(value << 4) & 0xF0, rs);
}

static void lcd_cmd(unsigned char cmd) {
    lcd_send(cmd, 0);
    if (cmd == 0x01 || cmd == 0x02) usleep(2000);
}

static void lcd_data(unsigned char data) { lcd_send(data, 1); }
static void lcd_clear(void) { lcd_cmd(0x01); }

static void lcd_set_cursor(int row, int col) {
    if (row < 0) row = 0;
    if (row >= g_rows) row = g_rows - 1;
    lcd_cmd((unsigned char)(0x80 | (row_offsets_20col[row] + col)));
}

static void lcd_print_line(int row, const char *text, int len) {
    char buf[64];
    if (len > g_cols) len = g_cols;
    if (g_cols >= (int)sizeof(buf)) len = 0;
    memset(buf, ' ', (size_t)g_cols);
    memcpy(buf, text, (size_t)len);
    lcd_set_cursor(row, 0);
    for (int i = 0; i < g_cols; i++) lcd_data((unsigned char)buf[i]);
}

static void lcd_print_str(int row, const char *text) {
    lcd_print_line(row, text, (int)strlen(text));
}

/* Definiert eines von 8 CGRAM-Zeichen (Index 0-7) aus einem 8-Byte-Muster.
 * Nach dem Schreiben von CGRAM-Daten steht der Adresszaehler im CGRAM-
 * Bereich - vor dem naechsten normalen Textausgabe-Aufruf setzt
 * lcd_print_line() die Cursorposition per lcd_set_cursor() ohnehin neu,
 * daher ist hier kein zusaetzlicher Reset noetig. */
static void lcd_create_char(int index, const unsigned char pattern[8]) {
    lcd_cmd((unsigned char)(0x40 | ((index & 0x07) << 3)));
    for (int i = 0; i < 8; i++) lcd_data(pattern[i] & 0x1F);
}

static void lcd_init(void) {
    usleep(50000);
    lcd_write4bits(0x30, 0); usleep(5000);
    lcd_write4bits(0x30, 0); usleep(200);
    lcd_write4bits(0x30, 0); usleep(200);
    lcd_write4bits(0x20, 0); usleep(200);
    lcd_cmd(0x28);
    lcd_cmd(0x0C);
    lcd_cmd(0x06);
    lcd_clear();
}

static int i2c_open(void) {
    int fd = open(g_bus_path, O_RDWR);
    if (fd < 0) { perror("open i2c bus"); exit(1); }
    if (ioctl(fd, I2C_SLAVE, g_i2c_addr) < 0) { perror("ioctl I2C_SLAVE"); exit(1); }
    return fd;
}

static void handle_signal(int sig) { (void)sig; stop_requested = 1; }

static void wait_seconds(int secs) {
    for (int s = 0; s < secs * 10 && !stop_requested; s++) usleep(100000);
}

/* --- 8 Beispiel-Icons (je 8 Byte = 8 Pixelzeilen, untere 5 Bit je Byte) --- */
static const unsigned char CHAR_HEART[8]  = {0b00000,0b01010,0b11111,0b11111,0b11111,0b01110,0b00100,0b00000};
static const unsigned char CHAR_SMILEY[8] = {0b00000,0b01010,0b01010,0b00000,0b10001,0b10001,0b01110,0b00000};
static const unsigned char CHAR_ARROW[8]  = {0b00000,0b00100,0b00010,0b11111,0b00010,0b00100,0b00000,0b00000};
static const unsigned char CHAR_CHECK[8]  = {0b00000,0b00001,0b00010,0b10100,0b01000,0b00000,0b00000,0b00000};
static const unsigned char CHAR_CROSS[8]  = {0b00000,0b10001,0b01010,0b00100,0b01010,0b10001,0b00000,0b00000};
static const unsigned char CHAR_BELL[8]   = {0b00100,0b01110,0b01110,0b01110,0b11111,0b00000,0b00100,0b00000};
static const unsigned char CHAR_NOTE[8]   = {0b00010,0b00011,0b00010,0b00010,0b01110,0b11110,0b01100,0b00000};
static const unsigned char CHAR_DEGREE[8] = {0b01100,0b10010,0b10010,0b01100,0b00000,0b00000,0b00000,0b00000};

static void demo_cgram(void) {
    lcd_create_char(0, CHAR_HEART);
    lcd_create_char(1, CHAR_SMILEY);
    lcd_create_char(2, CHAR_ARROW);
    lcd_create_char(3, CHAR_CHECK);
    lcd_create_char(4, CHAR_CROSS);
    lcd_create_char(5, CHAR_BELL);
    lcd_create_char(6, CHAR_NOTE);
    lcd_create_char(7, CHAR_DEGREE);

    lcd_clear();
    lcd_print_str(0, "Eigene Symbole:");
    lcd_set_cursor(1, 0);
    for (int i = 0; i < 8 && i * 2 < g_cols; i++) { lcd_data((unsigned char)i); lcd_data(' '); }
    if (g_rows > 2) lcd_print_str(2, "Herz Smiley Pfeil");
    if (g_rows > 3) lcd_print_str(3, "OK X Glocke Note Grad");
    wait_seconds(6);
}

static void demo_extended_rom(void) {
    const int page_size = g_rows * g_cols;
    const int start_code = 0x80;
    const int total = 0x100 - 0x80; /* 128 Codes */

    for (int start = 0; start < total && !stop_requested; start += page_size) {
        lcd_clear();
        char line[64];
        for (int row = 0; row < g_rows; row++) {
            for (int col = 0; col < g_cols && col < (int)sizeof(line); col++) {
                int idx = start + row * g_cols + col;
                line[col] = (idx < total) ? (char)(start_code + idx) : ' ';
            }
            lcd_print_line(row, line, g_cols);
        }
        wait_seconds(5);
    }
}

static void print_usage(const char *prog) {
    fprintf(stderr,
        "Verwendung: %s [-b BUSPFAD] [-a HEXADRESSE] [-r ZEILEN] [-c SPALTEN] [-1] [-h]\n"
        "  -b PFAD   I2C-Bus-Geraetedatei          (Standard: /dev/i2c-1)\n"
        "  -a HEX    I2C-Adresse des Displays       (Standard: 27)\n"
        "  -r N      Zeilenzahl des Displays        (Standard: 4)\n"
        "  -c N      Spaltenzahl des Displays        (Standard: 20)\n"
        "  -1        Nur einmal durchlaufen statt Endlosschleife\n"
        "  -h        Diese Hilfe anzeigen\n",
        prog);
}

int main(int argc, char **argv) {
    int opt;
    while ((opt = getopt(argc, argv, "b:a:r:c:1h")) != -1) {
        switch (opt) {
            case 'b': snprintf(g_bus_path, sizeof(g_bus_path), "%s", optarg); break;
            case 'a': g_i2c_addr = (int)strtol(optarg, NULL, 16); break;
            case 'r': g_rows = atoi(optarg); break;
            case 'c': g_cols = atoi(optarg); break;
            case '1': g_once = 1; break;
            case 'h': print_usage(argv[0]); return 0;
            default:  print_usage(argv[0]); return 1;
        }
    }
    if (g_rows < 1 || g_rows > 4 || g_cols < 1 || g_cols > 40) {
        fprintf(stderr, "Ungueltige Zeilen-/Spaltenzahl (unterstuetzt: 1-4 Zeilen, 1-40 Spalten)\n");
        return 1;
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    i2c_fd = i2c_open();
    lcd_init();

    lcd_print_str(0, "Grafik-Test");
    lcd_print_str(1, "1) CGRAM-Symbole");
    if (g_rows > 2) lcd_print_str(2, "2) ROM 0x80-0xFF");
    lcd_print_str(g_rows > 3 ? 3 : (g_rows - 1), "Start...");
    wait_seconds(2);

    do {
        demo_cgram();
        if (stop_requested) break;
        lcd_clear();
        lcd_print_str(0, "Erweiterter ROM:");
        if (g_rows > 1) lcd_print_str(1, "Codes 0x80-0xFF");
        wait_seconds(2);
        demo_extended_rom();
    } while (!g_once && !stop_requested);

    lcd_clear();
    lcd_print_str(0, "Test beendet.");
    close(i2c_fd);
    return 0;
}
