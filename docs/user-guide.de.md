# Anleitung

So installierst du den SC Localization Launcher, richtest ihn ein und behebst die üblichen Probleme. [English version](user-guide.md).

- [Installieren](#installieren)
- [Erster Start](#erster-start)
- [Das Hauptfenster](#das-hauptfenster)
- [Sprachen](#sprachen)
- [Updates](#updates)
- [Werkzeuge](#werkzeuge)
- [Probleme lösen](#probleme-lösen)
- [Einen Fehler melden](#einen-fehler-melden)

## Installieren

Lade die passende Datei von der [Release-Seite](https://github.com/xXCrusaderXx/SC-Localization-Launcher/releases); das [README](../README.md#installation) nennt den Installationsbefehl für jede.

- **AppImage**: läuft auf jeder Distribution mit glibc 2.35 oder neuer und aktualisiert sich selbst.
- **Flatpak** (auch Steam Deck): wird über `flatpak update` oder dein Software-Center aktualisiert.
- **deb, rpm, Arch-Paket**: über deinen Paketmanager installiert; der Launcher bietet neue Versionen an und installiert sie mit deinem Passwort.

Star Citizen muss mit dem [LUG Helper](https://github.com/starcitizen-lug/lug-helper), Lutris oder in einem anderen Wine-Prefix installiert sein. Das Spiel selbst installiert der Launcher nicht.

## Erster Start

Beim ersten Start öffnet sich der Einrichtungsassistent. Er hat vier Schritte, jeder lässt sich überspringen:

1. **Willkommen**: die Sprache des Launchers selbst.
2. **Star Citizen finden**: *Automatisch suchen* durchsucht deine Laufwerke nach dem RSI Launcher und den Spielversionen (LIVE, PTU, …). Wird nichts gefunden, wähle mit *Ordner wählen …* den Ordner der Spielversion, z. B. `…/StarCitizen/LIVE`.
3. **Übersetzung wählen**: die Sprache für Star Citizen, eingestellt für jede gefundene Spielversion.
4. **Fast fertig**: automatische Updates und Infobereich. *Jetzt installieren* lädt die Übersetzung herunter; das Spiel wird nicht gestartet.

Unter Einstellungen → Hilfe → *Einrichtungsassistent …* öffnest du ihn jederzeit wieder.

## Das Hauptfenster

Jede Spielversion mit Ordner hat eine Zeile:

- Der **Punkt** zeigt den Zustand: grün = aktuell, gelb = ein Update wartet, rot = die Dateien wurden verändert oder das letzte Update ist fehlgeschlagen.
- Die **Auswahl** legt die Übersetzung dieser Spielversion fest. *English* entfernt die Übersetzung wieder.
- **↻ Reparieren** lädt die Übersetzung erneut und schreibt ihre Zeilen in der `user.cfg` neu, z. B. wenn ein Spielupdate die Dateien ersetzt hat.
- **✎** ändert den Ordner der Spielversion.

Der große Button unten rechts installiert, was wartet. Ist *Star Citizen danach starten* an, startet er anschließend den RSI Launcher über das `sc-launch.sh` des LUG Helpers oder über Lutris; wartet nichts, startet er nur das Spiel.

Solange Star Citizen läuft, werden Übersetzungen nie verändert. Ein wartendes Update folgt, sobald das Spiel beendet ist.

## Sprachen

**Übersetzungen des Spiels** stammen von Community-Projekten wie [StarCitizen-Deutsch-INI](https://github.com/rjcncpt/StarCitizen-Deutsch-INI); Einstellungen → Übersetzungen nennt Projekt und Lizenz. Falsche oder fehlende Texte meldest du dort, nicht beim Launcher.

- Eine Sprache, die eine Quelle für eine Spielversion gerade nicht anbietet, wird angezeigt, lässt sich aber nicht wählen.
- *Eigene Quelle …* in der Auswahl nimmt die HTTPS-Adresse einer beliebigen `global.ini` und den Sprachordner, in den sie gehört.

**Item-Werte** (Einstellungen → Verhalten, beide standardmäßig aus) stammen aus dem [Star Citizen Wiki](https://star-citizen.wiki) und kommen mit dem nächsten Update in die installierte Übersetzung:

- *Größe, Klasse und Grad an Gegenstandsnamen anhängen*, z. B. „Holdstrong (S4 Militär A)“. Rüstungen bekommen ihre Gewichtsklasse, z. B. „ADP Core (Schwer)“, Mining-Laser ihre Größe.
- *Werte in Gegenstandsbeschreibungen einfügen*, z. B. „HP 100.000 · Regen 19.000/s“ bei Schilden. Rüstungen und Mining-Gegenstände behalten ihre Beschreibung, denn dort stehen ihre Werte im Spiel schon.

**Die Sprache des Launchers** stellst du unter Einstellungen → Verhalten → *Sprache des Launchers* ein: *Automatisch (Systemsprache)* oder English, Deutsch, Español, Français, Italiano, Nederlands, Polski, Português. *Darstellung* wählt ein dunkles oder helles Theme oder folgt dem System. Der *Streamer-Modus* blendet deinen Benutzernamen und Home-Ordner in angezeigten Pfaden aus, während du streamst oder aufnimmst.

## Updates

- **Übersetzungen**: Solange der Launcher offen ist, prüft er stündlich. Mit *Übersetzungen automatisch aktualisieren* installiert er neue Versionen selbst und wartet, falls das Spiel läuft, bis es beendet ist.
- **Infobereich**: Das Symbol zeigt denselben farbigen Punkt; sein Menü hat *Übersetzungen aktualisieren*, *Öffnen* und *Beenden*. Mit *Im Infobereich weiterlaufen* läuft der Launcher dort weiter, wenn du das Fenster schließt. GNOME braucht dafür die AppIndicator-Erweiterung.
- **Ohne Fenster**: `sc-localization-launcher --update` aktualisiert aus einem Skript oder einem `systemd --user`-Timer, siehe [Command line](../README.md#command-line).
- **Der Launcher selbst**: *Beim Start nach Launcher-Updates suchen* oder der ⇩-Button in der Titelleiste. Jedes Release ist signiert und wird vor der Installation geprüft.

## Werkzeuge

Alle unter Einstellungen → Werkzeuge.

- **Caches leeren …**: entfernt die Shader-Caches des Spiels, von DXVK/VKD3D, Mesa und NVIDIA. Hilft gegen Ruckler nach einem Spiel- oder Treiberupdate; der erste Start danach dauert länger. Grafikeinstellungen bleiben. Nicht, solange Star Citizen läuft.
- **Sicherungen …**: Tastenbelegung, Profileinstellungen, gespeicherte Charaktere und `user.cfg` pro Spielversion. Eine Wiederherstellung sichert zuerst, was sie ersetzt; solange Star Citizen läuft, wird sie abgelehnt. Die neuesten 10 Sicherungen pro Spielversion bleiben erhalten.
- **Spieleinstellungen …** pro Spielversion:
  - Bildratenbegrenzung im Spiel und in Menüs sowie eigene `Schlüssel = Wert`-Zeilen, geschrieben in die `user.cfg`. Nach dem nächsten Spielstart zeigt der Launcher, welche davon Star Citizen ignoriert hat („not whitelisted“).
  - die Grafik-API (Vulkan oder DirectX 11), geschrieben in die `GraphicsSettings.json` des Spiels. Sie wird geschrieben, wenn das Spiel nicht läuft, und erneut nach dem ersten Start jeder neuen Spielversion, die mit den Standardwerten des Spiels beginnt.
- **Programme …**: eigene Programme oder Skripte, die starten, sobald Star Citizen startet, z. B. ein Head-Tracking-Tool. Jedes lässt sich mit dem Spiel wieder beenden. Anführungszeichen wirken wie in einer Shell; für Variablen oder Pipes ein Skript oder `sh -c '…'` starten. Der Launcher muss dafür offen sein (der Infobereich reicht).

## Probleme lösen

**Nach einem Spielupdate ist das Spiel wieder englisch.**
Das Update hat die Übersetzung ersetzt; der Punkt der Spielversion ist rot. Klick auf ↻ *Reparieren* oder lass *Übersetzungen automatisch aktualisieren* das erledigen.

**Das Spiel bleibt englisch, obwohl die Übersetzung installiert ist.**
Prüfe, ob der Ordner der Spielversion der ist, aus dem das Spiel läuft (z. B. `…/StarCitizen/LIVE`, nicht `…/StarCitizen`). Einstellungen → Hilfe → *Installation prüfen …* zeigt, ob `global.ini` und die Sprachzeilen in der `user.cfg` stimmen. Starte Star Citizen komplett neu.

**„Spiel kann nicht gestartet werden: Installiere es mit dem LUG Helper oder Lutris“.**
Der Launcher startet das Spiel über das `sc-launch.sh` des LUG Helpers neben dem Wine-Prefix oder über Lutris. Bei einer anderen Installation startest du das Spiel selbst; die Übersetzung wird trotzdem installiert.

**„Der RSI Launcher ist nicht gestartet“.**
Der Link in der Meldung öffnet das Log. Das Ende von `sc-launch.log` (LUG Helper) sagt meist, warum. Ein Start direkt über den LUG Helper oder Lutris zeigt denselben Fehler oft deutlicher.

**Eine Spieleinstellung wirkt nicht.**
Steht sie im Dialog als ignoriert („not whitelisted“), nimmt Star Citizen sie aus der `user.cfg` nicht an. Die Grafik-API ändert sich nur, wenn das Spiel nicht läuft; der Dialog sagt, wenn sie noch wartet.

**Ein Start des Launchers holt nur ein offenes Fenster nach vorne.**
Es läuft immer nur ein Launcher. Ist er im Infobereich, öffnet ein zweiter Start diesen. Um eine andere Version zu starten, beende zuerst den laufenden: Infobereich → *Beenden*.

**Das Flatpak findet das Spiel nicht.**
Nimm *Ordner wählen …*. Das Flatpak darf alle Ordner lesen und startet das Spiel außerhalb seiner Sandbox.

## Einen Fehler melden

1. Einstellungen → Hilfe → *Installation prüfen …* → *Bericht speichern (ZIP)*. Er enthält die Prüfergebnisse, die Einstellungen und die Logs des Launchers, mit ersetztem Home-Ordner und Benutzernamen; `Game.log` ist nicht dabei.
2. Öffne ein [Issue](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues) und hänge das ZIP an.

Falsche oder fehlende Texte im Spiel gehören zum Übersetzungsprojekt, das unter Einstellungen → Übersetzungen steht.
