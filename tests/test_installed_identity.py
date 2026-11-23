#!/usr/bin/env python3
import configparser
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET


prefix = Path(sys.argv[1]).resolve()
data = prefix / sys.argv[2]
app_id = "io.github.mikepadiernos.plasma_impresario"
binary = prefix / "bin" / "plasma-impresario"
alias = prefix / "bin" / "trojita"
assert binary.is_file() and alias.samefile(binary)

env = dict(os.environ, QT_QPA_PLATFORM="offscreen")
for executable in (binary, alias):
    output = subprocess.check_output([str(executable), "--version"], env=env, text=True)
    assert output.startswith("Plasma Impresario "), output

for desktop_id in (app_id, "org.kde.trojita"):
    desktop = configparser.ConfigParser(interpolation=None, strict=True)
    desktop.optionxform = str
    desktop.read(data / "applications" / f"{desktop_id}.desktop")
    entry = desktop["Desktop Entry"]
    assert entry["Name"] == "Plasma Impresario"
    try_exec = entry["TryExec"].replace("\\s", " ").replace("\\\\", "\\")
    assert Path(try_exec) == binary
    assert shutil.which(try_exec, path="/usr/bin:/bin") == str(binary)
    exec_line = entry["Exec"].replace("\\\\", "\\").replace("\\s", " ")
    assert shlex.split(exec_line) == [str(binary).replace("%", "%%"), "%u"]
    assert entry["Icon"] == "plasma-impresario"
    composer = desktop["Desktop Action Composer"]["Exec"].replace("\\\\", "\\").replace("\\s", " ")
    assert shlex.split(composer) == [str(binary).replace("%", "%%"), "-c"]
    assert (entry.get("NoDisplay") == "true") == (desktop_id == "org.kde.trojita")

metadata = ET.parse(data / "metainfo" / f"{app_id}.metainfo.xml").getroot()
assert metadata.findtext("id") == app_id
assert metadata.findtext("name") == "Plasma Impresario"
assert metadata.findtext("launchable") == f"{app_id}.desktop"
assert {"plasma-impresario", "trojita"} <= {x.text for x in metadata.findall("provides/binary")}
assert (data / "icons/hicolor/scalable/apps/plasma-impresario.svg").is_file()
assert (data / "icons/hicolor/scalable/apps/trojita.svg").is_file()
print("Installed identity and compatibility aliases verified")
