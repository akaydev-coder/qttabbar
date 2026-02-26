# Offline-Kompilierung in einer Visual-Studio-2022-VM (Schritt für Schritt)

Diese Anleitung zeigt, wie du QTTabBar **ohne Internetzugang** in einer VS2022-VM kompilierst.

## 1) Einmalige Vorbereitung auf einem Online-Rechner

> Ziel: Alle benötigten Installer, Abhängigkeiten und den Quellcode vorab herunterladen.

1. Erstelle auf einem Online-Rechner einen Ordner, z. B. `D:\qttabbar-offline-kit`.
2. Lade Visual Studio 2022 als Offline-Layout herunter:
   ```bat
   vs_Community.exe --layout D:\qttabbar-offline-kit\vs2022-layout --lang en-US de-DE
   ```
3. Stelle sicher, dass folgende Workloads/Komponenten im Layout enthalten sind:
   - **.NET desktop development**
   - **Desktop development with C++**
   - **MSBuild**
   - **NuGet package manager**
   - **.NET Framework 3.5 targeting pack** (wichtig für ältere Projekte)
4. Lade den WiX Toolset Installer (v3.11) in den Offline-Kit-Ordner.
5. Lade die VSIX-Datei `NotifyPropertyWeaverVsPackage.vsix` (liegt bereits im Repo) ebenfalls in den Offline-Kit.
6. Lade den Repository-Quellcode als ZIP oder per `git clone` und kopiere ihn in den Offline-Kit.
7. Optional: Falls NuGet-Pakete extern benötigt werden, lade sie vorab mit:
   ```powershell
   nuget restore "QTTabBar Rebirth.sln" -PackagesDirectory D:\qttabbar-offline-kit\packages
   ```

## 2) Offline-VM vorbereiten

1. Kopiere `qttabbar-offline-kit` in die VM (ISO, Shared Folder oder USB).
2. Installiere Visual Studio 2022 **aus dem Offline-Layout**.
3. Installiere .NET Framework 3.5 in der VM (Windows-Feature oder Offline-Installer).
4. Installiere WiX Toolset 3.11.
5. Installiere `NotifyPropertyWeaverVsPackage.vsix` per Doppelklick.

## 3) Quellcode einrichten

1. Entpacke den Quellcode z. B. nach `D:\src\qttabbar`.
2. Öffne `QTTabBar Rebirth.sln` in Visual Studio 2022.
3. Stelle oben in VS ein:
   - `Configuration`: **Release**
   - `Platform`: **Any CPU** (oder projektspezifisch x86)
4. Prüfe fehlende Referenzen und verknüpfe ggf. lokale DLLs, falls VS warnt.

## 4) NuGet komplett offline nutzen

Wenn Restore aus dem Internet nicht möglich ist:

1. Öffne **Tools → NuGet Package Manager → Package Manager Settings**.
2. Füge eine lokale Quelle hinzu, z. B.:
   - Name: `offline-packages`
   - Source: `D:\qttabbar-offline-kit\packages`
3. Deaktiviere/entferne Online-Feeds für diesen Build.
4. Führe dann aus:
   ```powershell
   nuget restore "QTTabBar Rebirth.sln" -Source "D:\qttabbar-offline-kit\packages"
   ```

## 5) Build in VS2022 (GUI)

1. Rechtsklick auf die Solution → **Restore NuGet Packages**.
2. Danach: **Build → Rebuild Solution**.
3. Prüfe den Output auf Fehler.
4. Build-Artefakte findest du typischerweise unter:
   - `QTTabBar\bin\Release\`
   - weitere Projektordner jeweils unter `bin\Release\`

## 6) Build per Developer Command Prompt (empfohlen für reproduzierbare Builds)

Starte **x64 Native Tools Command Prompt for VS 2022** und führe aus:

```bat
cd /d D:\src\qttabbar
nuget restore "QTTabBar Rebirth.sln" -Source "D:\qttabbar-offline-kit\packages"
msbuild "QTTabBar Rebirth.sln" /t:Rebuild /m /p:Configuration=Release /p:Platform="Mixed Platforms" /p:VisualStudioVersion=17.0
```

## 7) Häufige Offline-Probleme

- **Fehlendes .NET 3.5 Targeting Pack**
  → In VS Installer (offline layout) das Targeting Pack nachinstallieren.

- **WiX-Projekt baut nicht**
  → Prüfen, ob WiX 3.11 korrekt installiert ist und die VS-Erweiterung geladen wurde.

- **NuGet kann Pakete nicht finden**
  → Offline-Quelle prüfen und sicherstellen, dass alle Pakete im lokalen Ordner liegen.

- **Fehlende/alte SDKs**
  → Das VS-Offline-Layout mit den benötigten Komponenten neu erstellen.

- **MSB8020 (fehlendes PlatformToolset)**
  → Prüfen, ob in C++ Projekten `PlatformToolset` auf `v143` steht und die VS2022 C++ Build Tools installiert sind.

- **MSB6011 / MSB4181 bei Post-Build-Events**
  → Build testweise mit deaktivierten Post-Build-Events starten:
  `msbuild "QTTabBar Rebirth.sln" /t:Rebuild /m /p:Configuration=Release /p:Platform="Mixed Platforms" /p:PostBuildEventUseInBuild=false`

## 8) Empfohlener Workflow

1. Online-Rechner: Layout + Pakete + Quellcode aktualisieren.
2. Offline-VM: Inhalte synchronisieren.
3. Immer denselben Build-Befehl verwenden (`msbuild ...`).
4. Artefakte versionieren (z. B. ZIP mit Tag/Datum).

---

Wenn du möchtest, kann ich als nächsten Schritt auch eine **komplette Checkliste als `build-offline.bat`** erstellen, damit du den gesamten Offline-Build in der VM mit einem Skript startest.


## 9) GitHub-Action-Artefakte sinnvoll verwenden

- `QTTabBar-project-components.zip` enthält die kompilierten Projekt-Komponenten inklusive nativer Binaries (`native/Win32` und `native/x64`).
- `QTTabBar.zip` (Tag-Release) enthält das veröffentlichte Paket inklusive nativer `QTTabBarNative`-Buildausgaben.
- Wenn der Offline-Build in der VM fehlschlägt, kannst du diese Artefakte als Fallback verwenden und in deiner Testumgebung entpacken.
