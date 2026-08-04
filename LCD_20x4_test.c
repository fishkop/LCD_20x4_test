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
 *   gcc -O2 -Wall -o LCD_20x4_test LCD_20x4_test.c
 *
 *   -O2    : Optimierungsstufe 2 (guter Kompromiss aus Laufzeit/Codegroesse,
 *            fuer dieses kleine Tool eigentlich unkritisch, aber Standard).
 *   -Wall  : alle gaengigen Compiler-Warnungen aktivieren (sauberer Code,
 *            deckt z. B. falsche Formatstrings oder unbenutzte Variablen auf).
 *   -o ... : Name der erzeugten ausfuehrbaren Datei.
 *   Keine weiteren -l/-I Optionen noetig - alles kommt aus der Standard-libc.
 *
 * Start-Optionen (Kommandozeile)
 * -------------------------------
 *   ./LCD_20x4_test [Optionen]
 *
 *   -b PFAD   I2C-Bus-Geraetedatei (Standard: /dev/i2c-1)
 *   -a HEX    I2C-Adresse des Displays in Hex, z. B. 27 oder 3f
 *             (Standard: 27 - per "i2cdetect -y 1" pruefen/ermitteln)
 *   -r N      Anzahl Zeilen des Displays (Standard: 4)
 *   -c N      Anzahl Spalten des Displays (Standard: 20)
 *   -d SEK    Anzeigedauer je Bildschirmseite in Sekunden (Standard: 4)
 *   -1        Nur einmal komplett durchlaufen, dann beenden
 *             (Standard: Endlosschleife, siehe unten)
 *   -h        Kurzhilfe anzeigen und beenden
 *
 *   Beispiele:
 *     ./LCD_20x4_test                      # Standardwerte, Endlosschleife
 *     ./LCD_20x4_test -a 3f -1              # andere Adresse, nur 1x durchlaufen
 *     ./LCD_20x4_test -r 2 -c 16 -d 3       # 16x2-Display, 3s je Seite
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
 * Stoppen des Hintergrundprozesses:
 *
 *   kill <PID>              # sauberes Beenden (SIGTERM), Display wird geloescht
 *   # oder falls die PID nicht mehr bekannt ist:
 *   pkill -f LCD_20x4_test
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
#include <signal.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

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

/* HD44780-Zeilenoffsets fuer 20-spaltige Displays (Zeile 3 knuepft direkt
 * an das Ende von Zeile 1 an, Zeile 4 an das Ende von Zeile 2 - so
 * adressiert der Controller intern ein 2-zeiliges DDRAM als 4 Zeilen). */
static const int row_offsets_20col[4] = {0x00, 0x40, 0x14, 0x54};

static int i2c_fd = -1;
static volatile sig_atomic_t stop_requested = 0;

static void i2c_write_byte(unsigned char data) {
    if (write(i2c_fd, &data, 1) != 1) perror("i2c write");
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

/* Setzt die Cursorposition (Zeile/Spalte, 0-basiert). Fuer Displays mit
 * anderer Spaltenzahl als 20 muesste die Offset-Tabelle angepasst werden
 * (16-spaltige Displays nutzen z. B. {0x00,0x40,0x10,0x50}). */
static void lcd_set_cursor(int row, int col) {
    if (row < 0) row = 0;
    if (row >= g_rows) row = g_rows - 1;
    lcd_cmd((unsigned char)(0x80 | (row_offsets_20col[row] + col)));
}

/* Schreibt eine Zeile Text, rechtsseitig mit Leerzeichen aufgefuellt bzw.
 * bei Bedarf abgeschnitten, damit alte Anzeigeninhalte sauber ueber-
 * schrieben werden (kein manuelles Clear noetig, kein Flackern). */
static void lcd_print_line(int row, const char *text, int len) {
    char buf[64];
    if (len > g_cols) len = g_cols;
    if (g_cols >= (int)sizeof(buf)) len = 0; /* Sicherheitsnetz bei Fehlkonfiguration */
    memset(buf, ' ', (size_t)g_cols);
    memcpy(buf, text, (size_t)len);
    lcd_set_cursor(row, 0);
    for (int i = 0; i < g_cols; i++) lcd_data((unsigned char)buf[i]);
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
    if (fd < 0) { perror("open i2c bus"); exit(1); }
    if (ioctl(fd, I2C_SLAVE, g_i2c_addr) < 0) { perror("ioctl I2C_SLAVE"); exit(1); }
    return fd;
}

/* SIGINT (Strg+C) und SIGTERM (z. B. von "kill"/"pkill") sauber abfangen,
 * damit das Display am Ende geloescht wird statt mitten im Bild
 * "einzufrieren". */
static void handle_signal(int sig) { (void)sig; stop_requested = 1; }

static void wait_seconds(int secs) {
    for (int s = 0; s < secs * 10 && !stop_requested; s++) usleep(100000);
}

static void print_usage(const char *prog) {
    fprintf(stderr,
        "Verwendung: %s [-b BUSPFAD] [-a HEXADRESSE] [-r ZEILEN] [-c SPALTEN]\n"
        "                [-d SEKUNDEN] [-1] [-h]\n"
        "  -b PFAD   I2C-Bus-Geraetedatei          (Standard: /dev/i2c-1)\n"
        "  -a HEX    I2C-Adresse des Displays       (Standard: 27)\n"
        "  -r N      Zeilenzahl des Displays        (Standard: 4)\n"
        "  -c N      Spaltenzahl des Displays        (Standard: 20)\n"
        "  -d SEK    Anzeigedauer je Seite in Sek.   (Standard: 4)\n"
        "  -1        Nur einmal durchlaufen statt Endlosschleife\n"
        "  -h        Diese Hilfe anzeigen\n",
        prog);
}

int main(int argc, char **argv) {
    int opt;
    while ((opt = getopt(argc, argv, "b:a:r:c:d:1h")) != -1) {
        switch (opt) {
            case 'b': snprintf(g_bus_path, sizeof(g_bus_path), "%s", optarg); break;
            case 'a': g_i2c_addr = (int)strtol(optarg, NULL, 16); break;
            case 'r': g_rows = atoi(optarg); break;
            case 'c': g_cols = atoi(optarg); break;
            case 'd': g_page_delay = atoi(optarg); break;
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

    lcd_print_line(0, "LCD Zeichentest", 15);
    lcd_print_line(1, "ASCII 0x20-0x7E", 16);
    char info[64];
    int infolen = snprintf(info, sizeof(info), "Bus %s @0x%02x", g_bus_path, g_i2c_addr);
    if (g_rows > 2) lcd_print_line(2, info, infolen);
    lcd_print_line(g_rows > 3 ? 3 : (g_rows - 1), "Start...", 8);
    wait_seconds(2);

    /* Druckbarer ASCII-Bereich: 0x20 (Space) bis 0x7E (~), 95 Zeichen. */
    const int first_code = 0x20;
    const int last_code = 0x7E;
    const int total_chars = last_code - first_code + 1;
    const int page_size = g_rows * g_cols;

    do {
        for (int start = 0; start < total_chars && !stop_requested; start += page_size) {
            lcd_clear();
            char line[64];
            for (int row = 0; row < g_rows; row++) {
                int pos = 0;
                for (int col = 0; col < g_cols && col < (int)sizeof(line); col++) {
                    int idx = start + row * g_cols + col;
                    line[pos++] = (idx < total_chars) ? (char)(first_code + idx) : ' ';
                }
                lcd_print_line(row, line, pos);
            }
            wait_seconds(g_page_delay);
        }
    } while (!g_once && !stop_requested);

    lcd_clear();
    lcd_print_line(0, "Test beendet.", 13);
    close(i2c_fd);
    return 0;
}
