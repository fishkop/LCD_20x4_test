/* =========================================================================
 * LCD_20x4_test.c
 *
 * Zweck
 * -----
 * Standalone-Testprogramm fuer ein HD44780-kompatibles Zeichen-LCD mit
 * PCF8574-I2C-Backpack (die handelsueblichen "IIC/I2C LCD"-Module).
 * Blaettert seitenweise durch den kompletten druckbaren ASCII-Bereich
 * (Buchstaben, Zahlen, Satz-/Sonderzeichen: 0x20 Space bis 0x7E Tilde),
 * damit man auf einen Blick pruefen kann, ob Verkabelung, I2C-Adresse
 * und Zeichensatz des Displays korrekt funktionieren.
 *
 * Kein externes Zubehoer/keine Bibliothek noetig ausser der libc und dem
 * Linux-Kernel-I2C-Interface (linux/i2c-dev.h) - laeuft unveraendert auf
 * jedem Linux-Board mit I2C-Bus (Raspberry Pi jeder Generation, andere
 * SBCs, ...). Ueber Kommandozeilenoptionen (siehe unten) lassen sich Bus,
 * I2C-Adresse und Displaygroesse anpassen, das Programm ist also nicht an
 * ein bestimmtes Projekt/Geraet gebunden.
 *
 * Hardware / Verkabelung (Standardfall, siehe auch lcd-status-hal7.md)
 * ---------------------------------------------------------------------
 *   LCD-Backpack-Pin | Pi GPIO-Header | Funktion
 *   ------------------|----------------|----------
 *   GND               | Pin 6 (GND)    | Masse
 *   VCC               | Pin 2/4 (5V)   | Stromversorgung
 *   SDA               | Pin 3 (GPIO2)  | I2C-Daten
 *   SCL               | Pin 5 (GPIO3)  | I2C-Takt
 *
 * Kompilieren
 * -----------
 *   gcc -O2 -Wall -Wextra -o LCD_20x4_test LCD_20x4_test.c
 *
 *   -O2      : Optimierungsstufe 2 (guter Kompromiss aus Laufzeit/Codegroesse,
 *              fuer dieses kleine Tool eigentlich unkritisch, aber Standard).
 *   -Wall    : alle gaengigen Compiler-Warnungen aktivieren.
 *   -Wextra  : zusaetzliche Warnungen (ungenutzte Parameter, Vorzeichen-
 *              vergleiche u. ae.) - dieses Programm uebersetzt damit
 *              warnungsfrei.
 *   -o ...   : Name der erzeugten ausfuehrbaren Datei.
 *   Keine weiteren -l/-I Optionen noetig - alles kommt aus der Standard-libc.
 *
 *   Optional zur Absicherung beim Testen neuer Aenderungen:
 *     gcc -O2 -Wall -Wextra -fsanitize=address,undefined \
 *         -o LCD_20x4_test LCD_20x4_test.c
 *
 * Start-Optionen (Kommandozeile)
 * -------------------------------
 *   ./LCD_20x4_test [Optionen]
 *
 *   -b PFAD   I2C-Bus-Geraetedatei (Standard: /dev/i2c-1, max. 255 Zeichen)
 *   -a HEX    I2C-Adresse des Displays in Hex, z. B. 27 oder 3f
 *             (Standard: 27; gueltig 03..77 - per "i2cdetect -y 1" ermitteln)
 *   -r N      Anzahl Zeilen des Displays   (Standard: 4;  gueltig 1..4)
 *   -c N      Anzahl Spalten des Displays  (Standard: 20; gueltig 1..40)
 *   -d SEK    Anzeigedauer je Bildschirmseite in Sekunden
 *             (Standard: 4; gueltig 0..3600)
 *   -1        Nur einmal komplett durchlaufen, dann beenden
 *             (Standard: Endlosschleife, siehe unten)
 *   -h        Kurzhilfe anzeigen und beenden
 *
 *   Alle Zahlenwerte werden streng geprueft: unvollstaendige oder nicht
 *   numerische Angaben (z. B. "-r 4x") und Werte ausserhalb der genannten
 *   Bereiche fuehren zu einer Fehlermeldung und Exit-Code 1, statt still
 *   auf einen Ersatzwert zurueckzufallen.
 *
 *   Beispiele:
 *     ./LCD_20x4_test                      # Standardwerte, Endlosschleife
 *     ./LCD_20x4_test -a 3f -1              # andere Adresse, nur 1x durchlaufen
 *     ./LCD_20x4_test -r 2 -c 16 -d 3       # 16x2-Display, 3s je Seite
 *     ./LCD_20x4_test -r 4 -c 16            # 16x4-Display (Zeilenoffsets
 *                                           # werden automatisch berechnet)
 *
 * Endlosschleife starten/stoppen
 * -------------------------------
 * Im Vordergrund starten (Ausgabe/Fehler direkt sichtbar, mit Strg+C
 * beenden - das Display wird dabei sauber geloescht):
 *
 *   ./LCD_20x4_test
 *   ^C                     # Strg+C: sauberer Abbruch via SIGINT-Handler
 *
 * Im Hintergrund weiterlaufen lassen (z. B. ueber eine SSH-Sitzung hinweg):
 *
 *   nohup ./LCD_20x4_test >lcd_test.log 2>&1 &
 *   echo $!                # gibt die PID aus, merken zum spaeteren Stoppen
 *
 * Stoppen (wichtig - siehe Fallstricke unten):
 *
 *   kill <PID>              # zuverlaessigste Variante, Display wird geloescht
 *   pkill -x LCD_20x4_test     # falls die PID nicht mehr bekannt ist
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
 *
 * Exit-Codes
 * ----------
 *   0  regulaeres Ende (auch nach Strg+C / SIGTERM)
 *   1  ungueltige Kommandozeilenoption oder Geraet nicht zu oeffnen
 *   2  Abbruch wegen wiederholter I2C-Schreibfehler (z. B. falsche Adresse
 *      oder Display nicht/nicht mehr angeschlossen) - fuer Skripte auswertbar

 *
 * Die Signalbehandlung nutzt sigaction() ohne SA_RESTART, damit ein
 * laufendes usleep() sofort abbricht und das Programm zuegig reagiert
 * (kein Warten bis zum Ende der aktuellen Seitenanzeige).
 *
 * Fuer einen dauerhaften Betrieb als System-Dienst (nicht nur zum Testen)
 * eignet sich eine systemd-Unit nach dem Vorbild von hal-lcd.service aus
 * lcd-status-hal7.md - fuer dieses reine Testprogramm i. d. R. nicht noetig.
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
#define MAX_DELAY_SEC 3600
#define I2C_ADDR_MIN  0x03   /* 0x00-0x02 sind reservierte I2C-Adressen */
#define I2C_ADDR_MAX  0x77   /* darueber liegen reservierte Adressen     */

/* Nach so vielen aufeinanderfolgenden I2C-Schreibfehlern wird abgebrochen,
 * statt in der Endlosschleife dauerhaft Fehlermeldungen zu produzieren
 * (z. B. wenn das Display im Betrieb abgezogen wird). */
#define MAX_CONSECUTIVE_I2C_ERRORS 20

/* ---- Standardwerte, per Kommandozeilenoption ueberschreibbar ---- */
static char g_bus_path[256] = "/dev/i2c-1";
static int  g_i2c_addr      = 0x27;
static int  g_rows          = 4;
static int  g_cols          = 20;
static int  g_page_delay    = 4;   /* Sekunden je Bildschirmseite */
static int  g_once          = 0;   /* 1 = nur einmal durchlaufen */

/* PCF8574 -> HD44780 Bit-Mapping (Standard-Backpack-Verdrahtung:
 * P7..P4 = D7..D4, P3 = Backlight, P2 = Enable, P1 = RW, P0 = RS) */
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

/* Erzeugt die auf-/ab-Flanke am Enable-Pin, die der HD44780 braucht, um
 * die anliegenden Datenleitungen zu uebernehmen. */
static void lcd_pulse_enable(unsigned char data) {
    i2c_write_byte(data | ENABLE_BIT);
    usleep(600);
    i2c_write_byte(data & (unsigned char)~ENABLE_BIT);
    usleep(600);
}

/* Sendet ein einzelnes Nibble (4 Bit) im 4-Bit-Modus des Controllers. */
static void lcd_write4bits(unsigned char nibble, unsigned char rs) {
    unsigned char data = (nibble & 0xF0) | LCD_BACKLIGHT | (rs ? RS_BIT : 0);
    i2c_write_byte(data);
    lcd_pulse_enable(data);
}

/* Sendet ein volles Byte als zwei Nibbles (High- dann Low-Nibble). */
static void lcd_send(unsigned char value, unsigned char rs) {
    lcd_write4bits(value & 0xF0, rs);
    lcd_write4bits((unsigned char)(value << 4) & 0xF0, rs);
}

static void lcd_cmd(unsigned char cmd) {
    lcd_send(cmd, 0);
    if (cmd == 0x01 || cmd == 0x02) usleep(2000); /* clear/home brauchen laenger */
}

static void lcd_data(unsigned char data) { lcd_send(data, 1); }
static void lcd_clear(void) { lcd_cmd(0x01); }

/* DDRAM-Startadresse einer Zeile. Der HD44780 verwaltet intern nur zwei
 * "logische" Zeilen; bei 4-zeiligen Displays setzen Zeile 3 und 4 die
 * Zeilen 1 und 2 direkt fort. Die Offsets haengen daher von der Spalten-
 * zahl ab (20x4 -> 0x00,0x40,0x14,0x54; 16x4 -> 0x00,0x40,0x10,0x50) und
 * werden hier berechnet statt fest verdrahtet. */
static int lcd_row_offset(int row) {
    switch (row) {
        case 0:  return 0x00;
        case 1:  return 0x40;
        case 2:  return 0x00 + g_cols;
        case 3:  return 0x40 + g_cols;
        default: return 0x00;
    }
}

/* Setzt die Cursorposition (Zeile/Spalte, 0-basiert), auf den gueltigen
 * Bereich des konfigurierten Displays begrenzt. */
static void lcd_set_cursor(int row, int col) {
    if (row < 0) row = 0;
    if (row >= g_rows) row = g_rows - 1;
    if (col < 0) col = 0;
    if (col >= g_cols) col = g_cols - 1;
    lcd_cmd((unsigned char)(0x80 | (lcd_row_offset(row) + col)));
}

/* Schreibt eine Zeile Text, rechtsseitig mit Leerzeichen aufgefuellt bzw.
 * bei Bedarf abgeschnitten, damit alte Anzeigeninhalte sauber ueber-
 * schrieben werden (kein manuelles Clear noetig, kein Flackern).
 *
 * len wird gegen 0 und gegen g_cols geklemmt; g_cols ist seinerseits durch
 * MAX_COLS begrenzt (Puffergroesse), damit hier unabhaengig von den
 * Aufrufern keine Pufferueberschreitung entstehen kann. */
static void lcd_print_line(int row, const char *text, int len) {
    char buf[MAX_COLS];
    if (text == NULL || len < 0) len = 0;
    if (len > g_cols) len = g_cols;
    memset(buf, ' ', (size_t)g_cols);
    if (len > 0) memcpy(buf, text, (size_t)len);
    lcd_set_cursor(row, 0);
    for (int i = 0; i < g_cols; i++) lcd_data((unsigned char)buf[i]);
}

/* Bequemer Wrapper: Laenge wird aus dem String selbst bestimmt, damit
 * Aufrufer keine Laengenangaben von Hand pflegen (und dabei falsch zaehlen)
 * muessen. */
static void lcd_print_str(int row, const char *text) {
    lcd_print_line(row, text, text ? (int)strlen(text) : 0);
}

/* Standard-Initialisierungssequenz fuer HD44780-Controller im 4-Bit-Modus
 * (siehe Hitachi-Datenblatt / unzaehlige Referenzimplementierungen). */
static void lcd_init(void) {
    usleep(50000);
    lcd_write4bits(0x30, 0); usleep(5000);
    lcd_write4bits(0x30, 0); usleep(200);
    lcd_write4bits(0x30, 0); usleep(200);
    lcd_write4bits(0x20, 0); usleep(200); /* auf 4-Bit-Modus umschalten */
    lcd_cmd(0x28); /* Function Set: 4-Bit, 2-Zeilen-Controller, 5x8 Font */
    lcd_cmd(0x0C); /* Display an, Cursor aus, Blinken aus */
    lcd_cmd(0x06); /* Entry Mode: Increment, kein Shift */
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

/* Setzt nur ein Flag - das ist die einzige hier zulaessige (async-signal-
 * sichere) Operation in einem Signal-Handler. */
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

/* Wartet in 100-ms-Schritten, damit ein Abbruchsignal zuegig wirkt.
 * Die Rechnung laeuft in long, damit auch der Maximalwert nicht ueberlaeuft. */
static void wait_seconds(int secs) {
    if (secs <= 0) return;
    for (long t = 0; t < (long)secs * 10 && !stop_requested; t++) usleep(100000);
}

/* Strenge Zahlenauswertung: akzeptiert nur vollstaendig numerische Angaben
 * im erlaubten Wertebereich, sonst Fehlermeldung und Rueckgabe -1. */
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

static void print_usage(const char *prog) {
    fprintf(stderr,
        "Verwendung: %s [-b BUSPFAD] [-a HEXADRESSE] [-r ZEILEN] [-c SPALTEN]\n"
        "                [-d SEKUNDEN] [-1] [-h]\n"
        "  -b PFAD   I2C-Bus-Geraetedatei          (Standard: /dev/i2c-1)\n"
        "  -a HEX    I2C-Adresse des Displays       (Standard: 27, gueltig 03..77)\n"
        "  -r N      Zeilenzahl des Displays        (Standard: 4,  gueltig 1..%d)\n"
        "  -c N      Spaltenzahl des Displays       (Standard: 20, gueltig 1..%d)\n"
        "  -d SEK    Anzeigedauer je Seite in Sek.  (Standard: 4,  gueltig 0..%d)\n"
        "  -1        Nur einmal durchlaufen statt Endlosschleife\n"
        "  -h        Diese Hilfe anzeigen\n",
        prog, MAX_ROWS, MAX_COLS, MAX_DELAY_SEC);
}

int main(int argc, char **argv) {
    int opt;

    while ((opt = getopt(argc, argv, "b:a:r:c:d:1h")) != -1) {
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
            case 'd':
                if (parse_int_arg(optarg, 10, 0, MAX_DELAY_SEC,
                                  "-d (Sekunden)", &g_page_delay) != 0) return 1;
                break;
            case '1': g_once = 1; break;
            case 'h': print_usage(argv[0]); return 0;
            default:  print_usage(argv[0]); return 1;
        }
    }

    install_signal_handlers();

    i2c_fd = i2c_open();
    lcd_init();

    /* Startbildschirm - Laengen kommen aus lcd_print_str(), nicht von Hand. */
    lcd_print_str(0, "LCD Zeichentest");
    if (g_rows > 1) lcd_print_str(1, "ASCII 0x20-0x7E");
    if (g_rows > 2) {
        /* Nur den Geraetenamen ohne Verzeichnis anzeigen (spart Platz auf dem
         * Display) und die Laenge explizit begrenzen, damit die Ausgabe
         * unabhaengig von der Laenge des -b Arguments beschraenkt bleibt. */
        const char *dev = strrchr(g_bus_path, '/');
        char info[MAX_COLS + 1];
        dev = dev ? dev + 1 : g_bus_path;
        snprintf(info, sizeof(info), "%.16s @0x%02x", dev, g_i2c_addr);
        lcd_print_str(2, info);
    }
    if (g_rows > 3) lcd_print_str(3, "Start...");
    wait_seconds(2);

    /* Druckbarer ASCII-Bereich: 0x20 (Space) bis 0x7E (~), 95 Zeichen. */
    const int first_code = 0x20;
    const int last_code = 0x7E;
    const int total_chars = last_code - first_code + 1;
    const int page_size = g_rows * g_cols;

    do {
        for (int start = 0; start < total_chars && !stop_requested; start += page_size) {
            lcd_clear();
            for (int row = 0; row < g_rows; row++) {
                char line[MAX_COLS];
                int pos = 0;
                for (int col = 0; col < g_cols; col++) {
                    int idx = start + row * g_cols + col;
                    line[pos++] = (idx < total_chars) ? (char)(first_code + idx) : ' ';
                }
                lcd_print_line(row, line, pos);
            }
            wait_seconds(g_page_delay);
        }
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
