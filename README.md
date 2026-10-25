# ESPHome-WRG-Display-Dashboard

Ein auf ESPHome, LVGL und dem **Adafruit Qualia ESP32-S3 for TTL RGB-666 Displays** basierendes Bar-Display-Dashboard für die Steuerung und Überwachung meiner Wohnraumlüftung (WRG).
Es wird das coole Bar Display (TL032FWV01CT-I1440A) mit einer Auflösung von 320 x 820 Pixel und Touch-Funktion verwendet.
Ich habe sonst keine OpenSource Projekte finden können, welche dieses Display & Controller unter ESPHome nutzen, daher hoffe ich, dass dieses Projekt anderen als Inspiration dienen kann.

---

## 📖 Projektbeschreibung

Dieses Projekt stellt ein modernes Touch-Dashboard zur Verfügung, welches lokal Sensordaten (BME680) erfasst und gleichzeitig als Fernbedienung und Anzeige für ein vernetzte Lüftungsgeräte ("ESPHome-Wohnraumlueftung") über Home Assistant dient.
Die Benutzeroberfläche wurde mit **LVGL (Light and Versatile Graphics Library)** umgesetzt und bietet flüssige Seitenübergänge und eine ansprechende Darstellung auf einem hochauflösenden "Rectangle Bar" Display.
(Achtung: Die LVGL-UI ist noch nicht fertiggestellt. Es gibt bereits eine funktionierende YAML-basierte UI, die aber noch nicht viel kann.)

## ✨ Features

- **Dreiseitiges LVGL-Dashboard im Querformat (820 x 320)** für [VentoSync](https://github.com/thomasengeroff-dotcom/ESPHome-Wohnraumlueftung)-Geräte (Seitenwechsel per Tippen, nach 60 s zurück zur Übersicht):
  - **Übersicht**: Uhrzeit, Raummodus, lokales Raumklima und je eine Karte pro Lüftungsgerät (online/aus, Lüfterstufe, Richtung Zuluft/Abluft, Drehzahl, Filter-Betriebstage bzw. Filterwechsel-Alarm) sowie der Smart-Automatik-Status des Masters.
  - **Wärmerückgewinnung**: Außenluft, Zuluft nach WRG, Raumluft und WRG-Effizienz des Geräts mit NTCs, dazu Radar-Anwesenheit, Klima-Koordination und Fenstersperre.
  - **Raumklima & System**: lokale BME680-Werte und pro Gerät Konfiguration (Gerät/Phase), WLAN-Signal und Firmware-Version.
- **Lokale Sensorik**: Einbindung eines lokalen BME680-Sensors zur Überwachung der Raumluftqualität am Ort des Displays.
- **Home Assistant Integration**: Nahtlose Anbindung an Home Assistant zum Lesen der Lüftungssensoren und -zustände.
- **Custom Display Initialisierung**: C++ basierte SPI-Initialisierung (Bitbanging) über einen I2C-Expander, da alle regulären Pins für das RGB-666 Interface benötigt werden.

## 🛠 Verwendete Hardware

Das Projekt nutzt eine sehr spezifische Hardware-Kombination, um das RGB-Display flüssig ansteuern zu können:

1. **Adafruit Qualia ESP32-S3 for TTL RGB-666 Displays**
   - Ein dediziertes Entwicklungsboard von Adafruit, das speziell für große RGB-Displays entwickelt wurde.
   - Bietet ausreichend PSRAM (Octal-Mode), was für den Framebuffer von LVGL bei solch großen Auflösungen zwingend erforderlich ist.
2. **Rectangle Bar RGB Display (TL032FWV01CT-I1440A)**
   - Auflösung: **320 x 820 Pixel** (Bar-Type Format)
   - Treiber-IC: **ST7701S**
   - Farbtiefe: RGB-666 (angesteuert als RGB565 in ESPHome)
   - *Anschluss:* Wird über ein 40-Pin FPC-Kabel direkt auf das Qualia-Board gesteckt.
3. **I2C-Expander (PCA9554 / TCA9554)**
   - Auf dem Adafruit Qualia Board verbaut (I2C-Adresse `0x3F`).
   - Da die 24-Bit RGB-Bilddaten nahezu alle GPIOs des ESP32-S3 belegen, wickelt dieser Expander die 3-Wire SPI Initialisierung (SCL, CS, RESET, SDA) sowie die Steuerung der Hintergrundbeleuchtung ab.
4. **FT63x6 Touch Controller**
   - Ebenfalls am I2C-Bus angeschlossen, ermöglicht kapazitives Touch auf dem kompletten Display. Das Polling wird asynchron in ESPHome ausgeführt, ohne den SPI-Init-Vorgang des Displays zu blockieren.
5. **BME680 Umweltsensor**
   - Angeschlossen über den standardmäßigen I2C-Bus (SDA: GPIO8, SCL: GPIO18) für genaue Temperatur-, Feuchte- und IAQ-Messungen.
   - *Tipp:* Das Qualia-Board besitzt einen **Stemma QT/Qwiic-Anschluss**, sodass der BME680 ganz ohne Löten oder Breadboard direkt per JST-SH Kabel angesteckt werden kann!

## ⚙️ Wie es mit ESPHome funktioniert (Besonderheiten)

Die Ansteuerung eines ST7701S RGB-Displays mit einem Adafruit Qualia Board unter ESPHome erfordert einige "Hacks", da ESPHome das Bitbanging von SPI-Befehlen über einen I2C-Expander im Standard-Display-Treiber nicht nativ unterstützt.

1. **RPI DPI RGB Plattform**: Das Display wird in `dashboard_main.yaml` als reguläres `rpi_dpi_rgb` Gerät definiert. ESPHome kümmert sich um den Pixel-Takt (16 MHz) und das reine Senden der RGB-Daten über die dedizierten R-, G- und B-Pins (z.B. Blue: `GPIO40`, `GPIO39` etc.).
2. **Custom Code in `qualia_init.h`**: Um den Display-Controller (ST7701S) beim Bootvorgang zu initialisieren (Konfiguration von Spannungen, Gamma-Werten, Color-Mapping etc.), wurde eine Custom C++ Komponente `QualiaDisplayInit` implementiert. Diese Komponente nutzt die zuvor im Ordner `docs` (`init-code-listings.txt`, `Initialization Codes.txt`) erarbeiteten Parameter und:
   - Spricht den PCA9554 I2C-Expander an.
   - Emuliert über Software ein 9-Bit SPI-Protokoll, indem SDA, SCL und CS am Expander im Millisekunden-Takt umgeschaltet werden.
   - Sendet die korrekte Sequenz an den ST7701S, noch bevor LVGL die Kontrolle übernimmt.
3. **LVGL Integration**: Die Benutzeroberfläche ist deklarativ in der `dashboard_ui.yaml` strukturiert und rendert dank Octal-PSRAM mit flüssigen Übergängen. Aktualisierungen der Labels werden sekündlich durch die Interval-Schleife der `dashboard_main.yaml` an die UI-Komponenten gepusht.

## 📂 Ordnerstruktur & Dateien

- `README.md` - Diese Dokumentation.
- `dashboard_main.yaml` - Die ESPHome Hauptkonfiguration (Board-Settings, WLAN, I2C, Sensoren, Display-Definition, OTA, Time).
- `dashboard_ui.yaml` - Die reine LVGL Benutzeroberfläche (Seiten `page_main`, `page_wrg`, `page_system`, Fonts, Styles).
- `packages/ventosync_device.yaml` - Home-Assistant-Entitäten eines VentoSync-Geräts, wird pro Gerät mit `key`/`prefix` eingebunden.
- `packages/ui_device_card.yaml`, `packages/ui_device_row.yaml` - Widget-Bausteine (Übersichtskarte, Systemzeile) pro Gerät.
- `dashboard.h` - C++-Helfer für NaN-sichere Formatierung und die Kartenlogik.
- `qualia_init.h` - Der C++ Code für die 3-Wire SPI Initialisierung des ST7701S via PCA9554 I2C-Expander.
- `secrets - Example.yaml` - Vorlage für die WLAN- und Passwortkonfigurationen.
- `docs/` - Datasheets, Timings, Layout-Vorgaben und Python-/Hex-Beispielcodes aus der Entwicklungs- und Prototyping-Phase der Displayansteuerung.

## 🚀 Setup & Installation

1. Kopiere die Datei `secrets - Example.yaml` zu `secrets.yaml` und trage dort deine WLAN-Zugangsdaten ein.
2. Trage in der `dashboard_main.yaml` unter `substitutions:` die HA-Entity-Präfixe deiner VentoSync-Geräte ein (`device_1_prefix` … `device_3_prefix`, z. B. `ventosync_dg_buero` für `fan.ventosync_dg_buero_ventosync_hrv`), dazu den Raum-Master (`master_prefix`) und das Gerät mit NTCs/Radar (`wrg_prefix`).
   - *Hinweis:* Neuere VentoSync-Entitäten (Automatik-Status, Klima-Koordination, Radar, Zuluft nach WRG) tragen zusätzlich den Bereichsnamen im Entity-ID-Präfix (z. B. `sensor.arbeitszimmer_ventosync_dg_buero_automatik_status`). Diese Präfixe stehen in `master_area_prefix` bzw. `wrg_area_prefix`.
3. Schließe das Display per FPC-Flachbandkabel an das Adafruit Qualia Board an und verbinde den BME680 (optional) über den Stemma QT Port.
4. Kompiliere und installiere die Firmware über USB (damit UART/JTAG korrekt eingebunden wird):

   ```bash
   esphome run dashboard_main.yaml
   ```

5. Nach dem erfolgreichen ersten Flashen können zukünftige Updates einfach via OTA-Update über dein lokales Netzwerk durchgeführt werden.
