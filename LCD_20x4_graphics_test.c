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
 *   gcc -O2 -Wall -Wextra -o LCD_20x4_gfx LCD_20x4_graphics_test.c
 *
 *   Gleiche Flags/Begruendung wie in LCD_20x4_test.c: -O2 Optimierung,
 *   -Wall/-Wextra alle Warnungen, keine externen Bibliotheken noetig.
 *
 *   Der kurze Name der ausfuehrbaren Datei (LCD_20x4_gfx, 12 Zeichen) ist
 *   Absicht: Linux fuehrt Prozessnamen nur bis 15 Zeichen, laengere Namen
 *   lassen sich mit "pkill -x" nicht mehr treffen (siehe Stoppen unten).
 *
 *   Uebersetzt mit -Wall -Wextra warnungsfrei. Zusaetzliches -Wpedantic
 *   meldet lediglich die Binaerliterale (0b...) der Symbol-Bitmaps weiter
 *   unten als GCC-Erweiterung - siehe Kommentar dort; das ist beabsichtigt
 *   (Lesbarkeit der Pixelmuster) und kein Fehler.
 *
 * Start-Optionen (Kommandozeile)
 * -------------------------------
 *   ./LCD_20x4_gfx [Optionen]
 *
 *   -b PFAD   I2C-Bus-Geraetedatei (Standard: /dev/i2c-1, max. 255 Zeichen)
 *   -a HEX    I2C-Adresse des Displays in Hex
 *             (Standard: 27; gueltig 03..77)
 *   -r N      Anzahl Zeilen des Displays   (Standard: 4;  gueltig 1..4)
 *   -c N      Anzahl Spalten des Displays  (Standard: 20; gueltig 1..40)
 *   -1        Nur einmal komplett durchlaufen, dann beenden
 *   -h        Kurzhilfe anzeigen und beenden
 *
 *   Alle Zahlenwerte werden streng geprueft: unvollstaendige oder nicht
 *   numerische Angaben und Werte ausserhalb der genannten Bereiche fuehren
 *   zu einer Fehlermeldung und Exit-Code 1.
 *
 * Endlosschleife starten/stoppen
 * -------------------------------
 * Vordergrund (mit Strg+C beenden, Display wird dabei geloescht):
 *   ./LCD_20x4_gfx
 *
 * Hintergrund (ueber SSH-Sitzung hinweg laufen lassen):
 *   nohup ./LCD_20x4_gfx >lcd_graphics_test.log 2>&1 &
 *   echo $!                    # PID merken
 *
 * Stoppen (wichtig - siehe Fallstricke unten):
 *
 *   kill <PID>              # zuverlaessigste Variante, Display wird geloescht
 *   pkill -x LCD_20x4_gfx      # falls die PID nicht mehr bekannt ist
 *
 * Fallstricke beim Beenden ueber den Namen
 * -----------------------------------------
 *   * NICHT "pkill -f <name>" verwenden. "-f" sucht in der kompletten
 *     Kommandozeile und trifft damit auch jede Shell bzw. jedes Skript, in
 *     deren Kommandozeile der Startbefehl vorkommt - ein Skript, das das
 *     Programm startet und spaeter wieder beenden will, beendet damit sich
 *     selbst (bzw. reisst eine SSH-Sitzung ab). Auch verankerte Muster
 *     helfen nicht zuverlaessig, weil der Startbefehl selbst passt.
 *   * "pkill -x" vergleicht den Prozessnamen exakt und ist deshalb sicher -
 *     funktioniert aber nur, wenn der Name hoechstens 15 Zeichen hat (Linux
 *     kuerzt das comm-Feld darauf). Deshalb wird oben bewusst ein kurzer
 *     Name fuer die ausfuehrbare Datei verwendet.
 *
 * Exit-Codes
 * ----------
 *   0  regulaeres Ende (auch nach Strg+C / SIGTERM)
 *   1  ungueltige Kommandozeilenoption oder Geraet nicht zu oeffnen
 *   2  Abbruch wegen wiederholter I2C-Schreibfehler (z. B. falsche Adresse
 *      oder Display nicht/nicht mehr angeschlossen) - fuer Skripte auswertbar

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
 * Angezeigt werden die eigenen Symbole ueber die Zeichencodes 0x00-0x07,
 * also z. B. lcd_data(0) fuer das erste definierte Symbol. Achtung beim
 * Einbau in eigene Programme: Code 0x00 ist in C zugleich das String-Ende,
 * eigene Symbole lassen sich daher nicht in normale C-Strings einbetten -
 * entweder direkt per lcd_data() ausgeben oder auf die Slots 1-7 auswichen.
 *
 * ========================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

/* ---- Harte Obergrenzen (begrenzen zugleich alle Puffergroessen) ---- */
#define MAX_ROWS        4
#define MAX_COLS       40
#define I2C_ADDR_MIN  0x03
#define I2C_ADDR_MAX  0x77
#define MAX_CONSECUTIVE_I2C_ERRORS 20

static char g_bus_path[256] = "/dev/i2c-1";
static int  g_i2c_addr      = 0x27;
static int  g_rows          = 4;
static int  g_cols          = 20;
static int  g_once          = 0;

#define LCD_BACKLIGHT 0x08
#define ENABLE_BIT    0x04
#define RS_BIT        0x01

static int i2c_fd = -1;
static volatile sig_atomic_t stop_requested = 0;
static int i2c_error_streak = 0;
static int i2c_failed = 0;   /* 1 = wegen I2C-Fehlern abgebrochen */

static void i2c_write_byte(unsigned char data) {
    if (write(i2c_fd, &data, 1) == 1) {
        i2c_error_streak = 0;
        return;
    }
    if (i2c_error_streak == 0) perror("i2c write");
    if (++i2c_error_streak >= MAX_CONSECUTIVE_I2C_ERRORS) {
        fprintf(stderr, "i2c write: %d Fehler in Folge - Abbruch.\n",
                i2c_error_streak);
        i2c_failed = 1;
        stop_requested = 1;
    }
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

/* DDRAM-Startadresse einer Zeile, abhaengig von der Spaltenzahl berechnet
 * (20x4 -> 0x00,0x40,0x14,0x54; 16x4 -> 0x00,0x40,0x10,0x50). */
static int lcd_row_offset(int row) {
    switch (row) {
        case 0:  return 0x00;
        case 1:  return 0x40;
        case 2:  return 0x00 + g_cols;
        case 3:  return 0x40 + g_cols;
        default: return 0x00;
    }
}

static void lcd_set_cursor(int row, int col) {
    if (row < 0) row = 0;
    if (row >= g_rows) row = g_rows - 1;
    if (col < 0) col = 0;
    if (col >= g_cols) col = g_cols - 1;
    lcd_cmd((unsigned char)(0x80 | (lcd_row_offset(row) + col)));
}

/* len wird gegen 0 und g_cols geklemmt; g_cols ist durch MAX_COLS (=
 * Puffergroesse) begrenzt, daher ist keine Pufferueberschreitung moeglich. */
static void lcd_print_line(int row, const char *text, int len) {
    char buf[MAX_COLS];
    if (text == NULL || len < 0) len = 0;
    if (len > g_cols) len = g_cols;
    memset(buf, ' ', (size_t)g_cols);
    if (len > 0) memcpy(buf, text, (size_t)len);
    lcd_set_cursor(row, 0);
    for (int i = 0; i < g_cols; i++) lcd_data((unsigned char)buf[i]);
}

static void lcd_print_str(int row, const char *text) {
    lcd_print_line(row, text, text ? (int)strlen(text) : 0);
}

/* Definiert eines von 8 CGRAM-Zeichen (Index 0-7) aus einem 8-Byte-Muster.
 * Nach dem Beschreiben des CGRAM steht der Adresszaehler des Controllers im
 * CGRAM-Bereich - ohne Ruecksetzen wuerde die naechste Zeichenausgabe dort
 * landen statt auf dem Bildschirm. Deshalb wird hier abschliessend wieder
 * eine DDRAM-Adresse gesetzt, damit die Funktion gefahrlos an beliebiger
 * Stelle aufgerufen werden kann. */
static void lcd_create_char(int index, const unsigned char pattern[8]) {
    lcd_cmd((unsigned char)(0x40 | ((index & 0x07) << 3)));
    for (int i = 0; i < 8; i++) lcd_data(pattern[i] & 0x1F);
    lcd_set_cursor(0, 0);
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
    if (fd < 0) {
        fprintf(stderr, "open %s: %s\n", g_bus_path, strerror(errno));
        exit(1);
    }
    if (ioctl(fd, I2C_SLAVE, g_i2c_addr) < 0) {
        fprintf(stderr, "ioctl I2C_SLAVE 0x%02x: %s\n", g_i2c_addr, strerror(errno));
        close(fd);
        exit(1);
    }
    return fd;
}

static void handle_signal(int sig) { (void)sig; stop_requested = 1; }

static void install_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; /* bewusst ohne SA_RESTART: usleep() soll abbrechen */
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static void wait_seconds(int secs) {
    if (secs <= 0) return;
    for (long t = 0; t < (long)secs * 10 && !stop_requested; t++) usleep(100000);
}

static int parse_int_arg(const char *s, int base, int min, int max,
                         const char *name, int *out) {
    char *end = NULL;
    long v;

    errno = 0;
    v = strtol(s, &end, base);
    if (errno != 0 || end == s || *end != '\0' || v < min || v > max) {
        /* Grenzen in derselben Zahlenbasis ausgeben wie die Eingabe erwartet
         * wird - sonst waere z. B. bei -a "erlaubt: 3..119" statt "03..77"
         * zu lesen und damit irrefuehrend. */
        if (base == 16) {
            fprintf(stderr, "Ungueltiger Wert fuer %s: '%s' (erlaubt: %02x..%02x, hexadezimal)\n",
                    name, s, (unsigned)min, (unsigned)max);
        } else {
            fprintf(stderr, "Ungueltiger Wert fuer %s: '%s' (erlaubt: %d..%d)\n",
                    name, s, min, max);
        }
        return -1;
    }
    *out = (int)v;
    return 0;
}

/* --- 8 Beispiel-Icons (je 8 Byte = 8 Pixelzeilen, untere 5 Bit je Byte) ---
 *
 * Geschrieben als Binaerliterale (0b...), weil sich die Pixelmuster so
 * unmittelbar ablesen und anpassen lassen: jede 1 ist ein gesetztes Pixel.
 * Hinweis zur Portabilitaet: 0b-Literale sind eine GCC-Erweiterung und erst
 * ab C23 offiziell im Standard. Fuer aeltere/andere Compiler koennen sie
 * 1:1 durch die entsprechenden Hexwerte ersetzt werden (z. B. 0b01010 =
 * 0x0A). Mit dem oben dokumentierten gcc-Aufruf funktioniert es direkt. */
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

    /* Symbole 0-7 einzeln ausgeben (nicht ueber einen C-String, da Code 0x00
     * dort das Stringende waere). Zeile bleibt innerhalb der Displaybreite. */
    if (g_rows > 1) {
        lcd_print_line(1, NULL, 0);            /* Zeile zuerst leeren */
        lcd_set_cursor(1, 0);
        for (int i = 0; i < 8 && (i * 2 + 1) < g_cols; i++) {
            lcd_data((unsigned char)i);
            lcd_data(' ');
        }
    }
    if (g_rows > 2) lcd_print_str(2, "Herz Smiley Pfeil OK");
    if (g_rows > 3) lcd_print_str(3, "X Glocke Note Grad");
    wait_seconds(6);
}

static void demo_extended_rom(void) {
    const int page_size = g_rows * g_cols;
    const int start_code = 0x80;
    const int total = 0x100 - 0x80; /* 128 Codes */

    for (int start = 0; start < total && !stop_requested; start += page_size) {
        lcd_clear();
        for (int row = 0; row < g_rows; row++) {
            char line[MAX_COLS];
            int pos = 0;
            for (int col = 0; col < g_cols; col++) {
                int idx = start + row * g_cols + col;
                line[pos++] = (idx < total) ? (char)(start_code + idx) : ' ';
            }
            lcd_print_line(row, line, pos);
        }
        wait_seconds(5);
    }
}

static void print_usage(const char *prog) {
    fprintf(stderr,
        "Verwendung: %s [-b BUSPFAD] [-a HEXADRESSE] [-r ZEILEN] [-c SPALTEN] [-1] [-h]\n"
        "  -b PFAD   I2C-Bus-Geraetedatei          (Standard: /dev/i2c-1)\n"
        "  -a HEX    I2C-Adresse des Displays       (Standard: 27, gueltig 03..77)\n"
        "  -r N      Zeilenzahl des Displays        (Standard: 4,  gueltig 1..%d)\n"
        "  -c N      Spaltenzahl des Displays       (Standard: 20, gueltig 1..%d)\n"
        "  -1        Nur einmal durchlaufen statt Endlosschleife\n"
        "  -h        Diese Hilfe anzeigen\n",
        prog, MAX_ROWS, MAX_COLS);
}

int main(int argc, char **argv) {
    int opt;

    while ((opt = getopt(argc, argv, "b:a:r:c:1h")) != -1) {
        switch (opt) {
            case 'b': {
                int n = snprintf(g_bus_path, sizeof(g_bus_path), "%s", optarg);
                if (n < 0 || (size_t)n >= sizeof(g_bus_path)) {
                    fprintf(stderr, "Bus-Pfad zu lang (max. %zu Zeichen)\n",
                            sizeof(g_bus_path) - 1);
                    return 1;
                }
                break;
            }
            case 'a':
                if (parse_int_arg(optarg, 16, I2C_ADDR_MIN, I2C_ADDR_MAX,
                                  "-a (I2C-Adresse)", &g_i2c_addr) != 0) return 1;
                break;
            case 'r':
                if (parse_int_arg(optarg, 10, 1, MAX_ROWS,
                                  "-r (Zeilen)", &g_rows) != 0) return 1;
                break;
            case 'c':
                if (parse_int_arg(optarg, 10, 1, MAX_COLS,
                                  "-c (Spalten)", &g_cols) != 0) return 1;
                break;
            case '1': g_once = 1; break;
            case 'h': print_usage(argv[0]); return 0;
            default:  print_usage(argv[0]); return 1;
        }
    }

    install_signal_handlers();

    i2c_fd = i2c_open();
    lcd_init();

    lcd_print_str(0, "Grafik-Test");
    if (g_rows > 1) lcd_print_str(1, "1) CGRAM-Symbole");
    if (g_rows > 2) lcd_print_str(2, "2) ROM 0x80-0xFF");
    if (g_rows > 3) lcd_print_str(3, "Start...");
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

    if (!i2c_failed) {
        lcd_clear();
        lcd_print_str(0, "Test beendet.");
    }
    close(i2c_fd);
    /* Exit-Code 2 signalisiert Skripten einen Hardware-/Busfehler,
     * 0 = regulaeres Ende (auch nach Strg+C). */
    return i2c_failed ? 2 : 0;
}
