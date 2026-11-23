#!/usr/bin/env python3
import os
from pathlib import Path
import subprocess
import sys
import tempfile


cmake, build, data_dir, cache_tool = sys.argv[1:]
icon_name = "io.github.mikepadiernos.plasma_impresario"

with tempfile.TemporaryDirectory() as temp:
    root = Path(temp)
    for destdir in ((True,) if Path(data_dir).is_absolute() else (False, True)):
        prefix = Path("/opt/plasma icon test") if destdir else root / "local prefix"
        stage = root / "stage" if destdir else Path("/")
        data_path = Path(data_dir) if Path(data_dir).is_absolute() else prefix / data_dir
        theme = stage / data_path.relative_to("/") / "icons/hicolor"
        apps = theme / "scalable/apps"
        apps.mkdir(parents=True)
        (apps / "existing.svg").write_text(
            '<svg xmlns="http://www.w3.org/2000/svg" width="1" height="1"/>'
        )
        subprocess.run([cache_tool, "--force", "--ignore-theme-index", "--quiet", str(theme)], check=True)
        cache = theme / "icon-theme.cache"
        assert icon_name.encode() + b"\0" not in cache.read_bytes()

        env = os.environ.copy()
        env.pop("DESTDIR", None)
        if destdir:
            env["DESTDIR"] = str(stage)
        subprocess.run(
            [cmake, "--install", build, "--prefix", str(prefix), "--component", "ApplicationIcon"],
            env=env,
            check=True,
        )
        assert (apps / f"{icon_name}.svg").is_file()
        assert icon_name.encode() + b"\0" in cache.read_bytes(), "Installed icon is missing from theme cache"

    empty_stage = root / "empty-stage"
    env = dict(os.environ, DESTDIR=str(empty_stage))
    subprocess.run(
        [cmake, "--install", build, "--prefix", "/opt/no cache", "--component", "ApplicationIcon"],
        env=env,
        check=True,
    )
    empty_data = Path(data_dir) if Path(data_dir).is_absolute() else Path("/opt/no cache") / data_dir
    empty_theme = empty_stage / empty_data.relative_to("/") / "icons/hicolor"
    assert (empty_theme / f"scalable/apps/{icon_name}.svg").is_file()
    assert not (empty_theme / "icon-theme.cache").exists()

    mock_bin = root / "bin"
    mock_bin.mkdir()
    failing_tool = mock_bin / "gtk-update-icon-cache"
    failing_tool.write_text("#!/bin/sh\necho 'cache refresh failed' >&2\nexit 1\n")
    failing_tool.chmod(0o755)
    env["PATH"] = str(mock_bin) + os.pathsep + env["PATH"]
    (empty_theme / "icon-theme.cache").touch()
    run = subprocess.run(
        [cmake, "--install", build, "--prefix", "/opt/no cache", "--component", "ApplicationIcon"],
        env=env,
        check=True,
        capture_output=True,
        text=True,
    )
    assert "Could not refresh icon cache" in run.stderr
    assert "cache refresh failed" in run.stderr
