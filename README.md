# LCD_20x4 Test-Tools — Übersicht

Zwei eigenständige, projektunabhängige C-Testprogramme für HD44780-
kompatible Zeichen-LCDs mit PCF8574-I2C-Backpack. Vollständig
dokumentiert direkt im Quellcode (Header-Kommentar am Dateianfang) —
diese Seite ist nur die Kurzreferenz. Erfolgreich getestet auf hal-9
mit einem 20x4-Display an `/dev/i2c-1`, Adresse `0x27`.

Beide Tools sind reines C ohne externe Bibliotheken (nur libc +
Linux-Kernel-I2C-Interface `linux/i2c-dev.h`) und laufen unverändert auf
jedem Linux-System mit I2C-Bus — nicht an hal-7/hal-9 gebunden, daher
auch in anderen Projekten wiederverwendbar. Bus, I2C-Adresse und
Displaygröße sind per Kommandozeilenoption einstellbar (siehe unten),
nicht im Code fest verdrahtet.

## Dateien

| Datei | Zweck |
|---|---|
| [`LCD_20x4_test.c`](LCD_20x4_test.c) | Zeichentest: blättert durch den kompletten druckbaren ASCII-Bereich (Buchstaben, Zahlen, Satz-/Sonderzeichen) |
| [`LCD_20x4_graphics_test.c`](LCD_20x4_graphics_test.c) | Grafiktest: eigene CGRAM-Symbole (Herz, Smiley, Pfeil, Haken, Kreuz, Glocke, Note, Gradzeichen) + Sweep durch den erweiterten ROM-Zeichensatz (Codes 0x80–0xFF) |

## Kompilieren

```bash
gcc -O2 -Wall -o LCD_20x4_test LCD_20x4_test.c
gcc -O2 -Wall -o LCD_20x4_graphics_test LCD_20x4_graphics_test.c
```

- `-O2` — Optimierungsstufe (Standard, für diese kleinen Tools unkritisch)
- `-Wall` — alle gängigen Compiler-Warnungen aktivieren
- keine weiteren `-l`/`-I`-Optionen nötig

## Start-Optionen (beide Tools identisch, bis auf `-d`)

| Option | Bedeutung | Standard |
|---|---|---|
| `-b PFAD` | I2C-Bus-Gerätedatei | `/dev/i2c-1` |
| `-a HEX` | I2C-Adresse des Displays (Hex) | `27` |
| `-r N` | Zeilenzahl des Displays | `4` |
| `-c N` | Spaltenzahl des Displays | `20` |
| `-d SEK` | Anzeigedauer je Seite (nur `LCD_20x4_test`) | `4` |
| `-1` | Nur einmal durchlaufen statt Endlosschleife | aus |
| `-h` | Kurzhilfe anzeigen | — |

Beispiele:
```bash
./LCD_20x4_test                     # Standardwerte, Endlosschleife
./LCD_20x4_test -a 3f -1             # andere I2C-Adresse, nur 1x durchlaufen
./LCD_20x4_test -r 2 -c 16 -d 3      # z. B. für ein 16x2-Display
./LCD_20x4_graphics_test -1          # Grafiktest, ein Durchlauf
```

Die Adresse vorab mit `i2cdetect -y 1` ermitteln, falls unklar (`i2c-tools`
muss installiert sein: `sudo apt install -y i2c-tools`; Benutzer sollte in
der Gruppe `i2c` sein: `sudo usermod -aG i2c $USER`).

## Endlosschleife starten / stoppen

**Vordergrund** (Ausgabe direkt sichtbar, mit `Strg+C` beenden — Display
wird dabei sauber gelöscht, kein "eingefrorenes" Bild):

```bash
./LCD_20x4_test
^C
```

**Hintergrund** (läuft über das Ende der SSH-Sitzung hinaus weiter):

```bash
nohup ./LCD_20x4_test >lcd_test.log 2>&1 &
echo $!                 # PID merken, um es später zu stoppen
```

**Stoppen:**

```bash
kill <PID>               # sauberes Beenden per SIGTERM, Display wird geloescht
# falls PID nicht mehr bekannt:
pkill -f LCD_20x4_test
pkill -f LCD_20x4_graphics_test
```

Beide Programme fangen `SIGINT` (Strg+C) und `SIGTERM` (`kill`/`pkill`) ab
und löschen das Display sauber, bevor sie sich beenden — kein hartes
`kill -9` nötig.

## Für dauerhaften Betrieb (nicht nur zum Testen)

Diese beiden Tools sind bewusst als **Testprogramme** gedacht (Start per
Hand, `nohup` für längere Läufe). Für einen dauerhaft laufenden
Status-Dienst (wie das spätere PostgreSQL/Patroni-Statusdisplay) eignet
sich stattdessen eine systemd-Unit nach dem Vorbild von `hal-lcd.service`
aus [`lcd-status-hal7.md`](lcd-status-hal7.md).

## Eigene CGRAM-Symbole anpassen

In `LCD_20x4_graphics_test.c` stehen die 8 Beispiel-Icons als einfache
8-Byte-Arrays (`CHAR_HEART`, `CHAR_SMILEY`, …). Jedes Byte ist eine
Pixelzeile des 5x8-Zeichens (untere 5 Bit zählen). Eigene Symbole lassen
sich z. B. mit einem der gängigen Online-"HD44780 Custom Character
Generator"-Tools entwerfen und als weiteres 8-Byte-Array eintragen
(maximal 8 Symbole gleichzeitig, der Controller hat nur 8 CGRAM-Slots).
