#!/usr/bin/env python3
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

"""Run the CI workflow locally with act using lightweight defaults."""

from __future__ import annotations

import argparse
import json
import os
import platform
import re
import math
import shlex
import shutil
import stat
import subprocess
import sys
import tarfile
import textwrap
from collections import deque, OrderedDict
from datetime import datetime
import urllib.error
import urllib.request
from pathlib import Path
from typing import Dict, List, Optional, TextIO


class TextUI:
    COLOR = {
        "reset": "\033[0m",
        "info": "\033[36m",
        "warn": "\033[33m",
        "error": "\033[31m",
        "ok": "\033[32m",
        "section": "\033[35m",
        "command": "\033[34m",
    }
    EMOJI = {
        "info": "💬 ",
        "warn": "⚠️ ",
        "error": "⛔ ",
        "ok": "✅ ",
        "section": "",
        "command": "▶️ ",
    }

    def __init__(self, enable_color: bool = True, enable_emoji: bool = True):
        force_color = os.environ.get("BOOTSTRAP_FORCE_COLOR") or os.environ.get("CLICOLOR_FORCE")
        force_emoji = os.environ.get("BOOTSTRAP_FORCE_EMOJI")
        self.color_enabled = bool(enable_color and (force_color or self._supports_color()))
        self.emoji_enabled = bool(enable_emoji and (force_emoji or self._supports_emoji()))

    @staticmethod
    def _supports_color() -> bool:
        if os.environ.get("NO_COLOR") or os.environ.get("BOOTSTRAP_PLAIN"):
            return False
        return True

    @staticmethod
    def _supports_emoji() -> bool:
        if os.environ.get("BOOTSTRAP_PLAIN"):
            return False
        return True

    def _fmt(self, text: str, kind: str, icon: Optional[str] = None) -> str:
        prefix = ""
        if self.emoji_enabled:
            prefix = icon if icon is not None else self.EMOJI.get(kind, "")
        if not self.color_enabled:
            return f"{prefix}{text}"
        color = self.COLOR.get(kind, "")
        reset = self.COLOR["reset"]
        return f"{color}{prefix}{text}{reset}"

    def info(self, msg: str, icon: Optional[str] = None) -> None:
        print(self._fmt(msg, "info", icon))

    def warn(self, msg: str, icon: Optional[str] = None) -> None:
        print(self._fmt(msg, "warn", icon))

    def error(self, msg: str, icon: Optional[str] = None) -> None:
        print(self._fmt(msg, "error", icon))

    def ok(self, msg: str, icon: Optional[str] = None) -> None:
        print(self._fmt(msg, "ok", icon))

    def section_big(self, title: str, icon: Optional[str] = None) -> None:
        prefix = (icon + " ") if (self.emoji_enabled and icon) else ""
        text = f"{prefix}{title}"
        line = "=" * max(8, len(text) + 8)
        banner = f"{line}\n|| {text}\n{line}"
        print(self._fmt(banner, "section", ""))

    def section_small(self, title: str, icon: Optional[str] = None) -> None:
        prefix = (icon + " ") if (self.emoji_enabled and icon) else ""
        banner = f"-- {prefix}{title} --"
        print(self._fmt(banner, "section", ""))

    def command(self, cmd: str, icon: Optional[str] = None) -> None:
        print(self._fmt(cmd, "command", icon))

    def kv(self, key: str, value: str, icon: Optional[str] = None) -> None:
        print(self._fmt(f"{key}: {value}", "info", icon))


ui = TextUI()

# Resolve to the repository root (this file lives in <root>/util/)
# parents[1] = repo root for paths like <root>/util/run_ci_with_act2.py
ROOT = Path(__file__).resolve().parent.parent
WORKFLOW_FILE = ROOT / ".github" / "workflows" / "ci.yml"
STATE_ROOT = ROOT / "local" / "act"
BIN_DIR = STATE_ROOT / "bin"
DOWNLOAD_DIR = STATE_ROOT / "downloads"
CACHE_DIR = STATE_ROOT / "cache"
ARTIFACT_DIR = STATE_ROOT / "artifacts"
TMP_DIR = STATE_ROOT / "tmp"
LOG_DIR = STATE_ROOT / "logs"
DEFAULT_SECRETS_FILE = STATE_ROOT / "secrets.env"
DEFAULT_ACT_VERSION = "v0.2.68"
DEFAULT_TOKEN = "local-test-token"
DEFAULT_PLATFORM_IMAGES = {
    # Use smaller images by default to reduce pull size; override with --image for full if needed.
    "ubuntu-latest": "ghcr.io/catthehacker/ubuntu:act-24.04",
    "ubuntu-24.04": "ghcr.io/catthehacker/ubuntu:act-24.04",
    "ubuntu-22.04": "ghcr.io/catthehacker/ubuntu:act-22.04",
    "ubuntu-20.04": "ghcr.io/catthehacker/ubuntu:act-20.04",
}
ARM_FRIENDLY_PLATFORM_IMAGES = DEFAULT_PLATFORM_IMAGES
FALLBACK_PLATFORM_IMAGES = {
    "ubuntu-latest": "ghcr.io/catthehacker/ubuntu:act-24.04",
    "ubuntu-24.04": "ghcr.io/catthehacker/ubuntu:act-24.04",
    "ubuntu-22.04": "ghcr.io/catthehacker/ubuntu:act-22.04",
    "ubuntu-20.04": "ghcr.io/catthehacker/ubuntu:act-20.04",
}


class ActError(RuntimeError):
    """Raised when act could not be prepared or run."""


def _prefix(message: str) -> str:
    timestamp = datetime.now().strftime("%H:%M:%S")
    return f"[{timestamp}] [run-ci-act] {message}"


def is_placeholder_token(token: str) -> bool:
    if not token:
        return True
    lower = token.lower()
    return token == DEFAULT_TOKEN or lower.startswith("local-") or lower == "ghp_placeholder"


def log(message: str, *, emoji: str = "ℹ️", color: str | None = None, bold: bool = False) -> None:
    text = _prefix(message)
    icon = f"{emoji} "
    if color == "yellow":
        ui.warn(text, icon=icon)
    elif color == "green":
        ui.ok(text, icon=icon)
    elif color == "red":
        ui.error(text, icon=icon)
    else:
        ui.info(text, icon=icon)


def warn(message: str) -> None:
    log(f"warning: {message}", emoji="⚠️", color="yellow")


def success(message: str) -> None:
    log(message, emoji="✅", color="green")


def error(message: str) -> None:
    log(message, emoji="⛔", color="red")


def ensure_directories() -> None:
    for path in (STATE_ROOT, BIN_DIR, DOWNLOAD_DIR, CACHE_DIR, ARTIFACT_DIR, TMP_DIR, LOG_DIR):
        path.mkdir(parents=True, exist_ok=True)


def check_docker() -> str:
    path = shutil.which("docker")
    if path:
        log(f"docker CLI found at {path}", emoji="🐳", color="blue")
        return path

    system = platform.system()
    if system == "Darwin":
        hint = "Install Docker Desktop for Mac (https://docs.docker.com/desktop/install/mac/) and start it once."
    elif system == "Linux":
        hint = "Install Docker Engine (e.g., via your package manager or https://docs.docker.com/engine/install/) and ensure your user can run docker commands."
    else:
        hint = "Install Docker Desktop/Engine for your OS and ensure the docker CLI is on PATH."

    raise ActError(f"docker is required by act but was not found. {hint} After installing, verify with `docker ps`.")


def detect_platform() -> tuple[str, str]:
    system = platform.system()
    machine = platform.machine().lower()
    if system not in {"Linux", "Darwin"}:
        raise ActError(f"Unsupported OS '{system}'. This script only supports Linux and macOS hosts.")
    if machine in {"x86_64", "amd64"}:
        arch = "x86_64"
    elif machine in {"arm64", "aarch64"}:
        arch = "arm64"
    else:
        raise ActError(f"Unsupported CPU architecture '{platform.machine()}'.")
    return system, arch


def resolve_container_arch(machine: str) -> str:
    # Prefer amd64 containers even on arm hosts to mirror GitHub’s runners and avoid
    # actions that download x86-only tooling (e.g., setup-ninja v5).
    return "linux/amd64" if machine in {"x86_64", "arm64"} else "linux/arm64"


def fetch_latest_act_tag(timeout: int = 20) -> str:
    url = "https://api.github.com/repos/nektos/act/releases/latest"
    req = urllib.request.Request(url, headers={"Accept": "application/vnd.github+json"})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as response:
            data = json.loads(response.read().decode("utf-8"))
        tag = data.get("tag_name")
        if isinstance(tag, str) and tag:
            return tag
        warn("latest release response missing tag_name; falling back to default version")
    except Exception as exc:  # noqa: BLE001 - best effort fetch
        warn(f"could not query GitHub releases: {exc}; falling back to {DEFAULT_ACT_VERSION}")
    return DEFAULT_ACT_VERSION


def download_act_archive(tag: str, system: str, arch: str) -> Path:
    suffix = "tar.gz" if system in {"Linux", "Darwin"} else "zip"
    asset_name = f"act_{system}_{arch}.{suffix}"
    url = f"https://github.com/nektos/act/releases/download/{tag}/{asset_name}"
    dest = DOWNLOAD_DIR / asset_name
    log(f"downloading {asset_name} ({tag})")
    try:
        with urllib.request.urlopen(url) as response:
            dest.write_bytes(response.read())
    except urllib.error.HTTPError as exc:
        raise ActError(f"failed to download {asset_name}: {exc}") from exc
    return dest


def install_act(tag: Optional[str]) -> Path:
    system, arch = detect_platform()
    version = tag or fetch_latest_act_tag()
    archive = download_act_archive(version, system, arch)
    with tarfile.open(archive) as tar:
        members = [m for m in tar.getmembers() if m.isfile() and m.name.endswith("act")]
        if not members:
            raise ActError(f"could not find act binary inside {archive.name}")
        member = members[0]
        binary_path = BIN_DIR / "act"
        with tar.extractfile(member) as src, open(binary_path, "wb") as dst:
            assert src is not None
            shutil.copyfileobj(src, dst)
    binary_path.chmod(binary_path.stat().st_mode | stat.S_IEXEC)
    log(f"installed act {version} to {binary_path}")
    return binary_path


def find_act(existing_path: Optional[str], preferred_version: Optional[str], *, allow_install: bool = True) -> Path:
    if existing_path:
        resolved = Path(existing_path)
        if resolved.is_file():
            log(f"using provided act binary at {resolved}", emoji="🧭", color="blue")
            return resolved
        raise ActError(f"Specified act path {existing_path} does not exist")

    path_from_env = shutil.which("act")
    if path_from_env:
        log(f"using act from PATH: {path_from_env}", emoji="🧭", color="blue")
        return Path(path_from_env)

    local_binary = BIN_DIR / "act"
    if local_binary.exists():
        log(f"using cached act at {local_binary}", emoji="🧭", color="blue")
        return local_binary

    if not allow_install:
        warn("act not found locally; using placeholder path because installation is disabled (likely due to --dry-run).")
        return Path("act")

    log("act not found; downloading...", emoji="⬇️", color="blue")
    return install_act(preferred_version)


def ensure_secrets_file(path: Path) -> Path:
    if not path.exists():
        log(f"creating default secrets file at {path}")
        content = textwrap.dedent(
            f"""
            GITHUB_TOKEN={DEFAULT_TOKEN}
            DEV_WEBSITES_SSH_KEY=
            GH_TOKEN={DEFAULT_TOKEN}
            """.strip()
        )
        path.write_text(content + "\n", encoding="utf-8")
        warn(
            "Edit local/act/secrets.env to add a real GitHub token (read-only is fine) "
            "so actions and registry pulls can authenticate."
        )
    return path


def parse_job_ids(workflow: Path) -> List[str]:
    if not workflow.exists():
        raise ActError(f"Workflow file {workflow} not found.")
    text = workflow.read_text(encoding="utf-8")
    try:
        import yaml  # type: ignore

        data = yaml.safe_load(text)
        jobs = list((data or {}).get("jobs", {}).keys())
        if jobs:
            return jobs
    except Exception:
        # Fall back to a simple regex-based scan if PyYAML is unavailable.
        pass

    jobs: List[str] = []
    in_jobs = False
    for line in text.splitlines():
        stripped = line.rstrip("\n")
        if not in_jobs:
            if stripped.strip().startswith("jobs:"):
                in_jobs = True
            continue
        m = re.match(r"^\s{2}([A-Za-z0-9_-]+):\s*$", stripped)
        if m:
            jobs.append(m.group(1))
    if not jobs:
        raise ActError(f"No jobs found under 'jobs:' in {workflow}")
    return jobs


def select_default_images(container_arch: str) -> Dict[str, str]:
    if container_arch == "linux/arm64":
        log("Detected arm64; using arm-friendly runner images by default.", emoji="🧠", color="green")
        return ARM_FRIENDLY_PLATFORM_IMAGES.copy()
    return DEFAULT_PLATFORM_IMAGES.copy()


def build_platform_overrides(overrides: List[str], base: Dict[str, str]) -> Dict[str, str]:
    images = base.copy()
    for item in overrides:
        if "=" not in item:
            raise ActError(f"Invalid --image override '{item}'. Expected label=image")
        label, image = item.split("=", 1)
        images[label.strip()] = image.strip()
    return images


def parse_runs_on_labels(workflow: Path) -> List[str]:
    """Best-effort extraction of static runs-on labels from the workflow YAML."""
    if not workflow.exists():
        return []
    labels: List[str] = []
    for raw_line in workflow.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line.startswith("runs-on:"):
            continue
        _, rhs = line.split(":", 1)
        rhs = rhs.strip()
        if not rhs or "${{" in rhs:
            continue
        # Handle single value or simple list form (runs-on: [ubuntu-latest])
        rhs = rhs.strip("[] ")
        for part in rhs.split(","):
            val = part.strip().strip("'\"")
            if val:
                labels.append(val)
    return labels


def docker_pull(
        image: str,
        dry_run: bool,
        label: str,
        token_source: Optional[str],
        gh_user: Optional[str],
        gh_scopes: Optional[List[str]],
        container_arch: str,
) -> str:
    cmd = ["docker", "pull", image]
    try:
        run_command(cmd, dry_run=dry_run, capture_output=True)
        success(f"image ready: {label} -> {image}")
        return image
    except ActError as exc:
        reason = str(exc)
        reason_note = f" (reason: {reason})" if reason else ""
        reason_lower = reason.lower()
        manifest_missing = "manifest unknown" in reason_lower or "manifest list entries" in reason_lower
        if "no space left on device" in reason_lower:
            warn(
                f"could not pull {image}{reason_note}. The Docker cache may be too large. "
                f"Try `docker system prune -af` (removes all unused images/containers) or `docker image prune -af`, then rerun. "
                "You can also pass a smaller image via --image ubuntu-24.04=<tag>."
            )
            raise
        ghcr_image = "ghcr.io/nektos/act-environments-ubuntu"
        if image.startswith(ghcr_image) and not dry_run:
            fallback = FALLBACK_PLATFORM_IMAGES.get(label)
            scope_note = ""
            missing_read_packages = False
            if gh_scopes is not None:
                joined = ", ".join(gh_scopes) if gh_scopes else "none"
                if not any(scope.lower() == "read:packages" for scope in gh_scopes):
                    missing_read_packages = True
                    scope_note = f" Scopes detected: {joined}. GHCR requires read:packages."
                else:
                    scope_note = f" Scopes detected: {joined}."
            if missing_read_packages:
                warn(
                    f"could not pull {image}{reason_note} (GHCR may require login). Trying a broader compatibility image instead.{scope_note}"
                )
                warn(
                    "Your current token source lacks read:packages. Create a PAT with read:packages (and repo for private repos) at "
                    "https://github.com/settings/tokens?type=classic, then login with it "
                    f"using: {login_hint(None, gh_user)}. "
                    "To avoid using the GitHub CLI token for pulls, set GH_TOKEN/GITHUB_TOKEN to that PAT (or add it to local/act/secrets.env) and rerun."
                )
            else:
                if manifest_missing:
                    warn(
                        f"could not pull {image}{reason_note}. The tag may not be published for {container_arch}. "
                        "Trying a broader compatibility image instead. "
                        "Override with --image <label>=<image> to force a different tag if needed."
                    )
                else:
                    extra = ""
                    if token_source:
                        extra = " If you recently logged in with a different credential, try `docker logout ghcr.io` and login again."
                    warn(
                        f"could not pull {image}{reason_note} (GHCR may require login). "
                        "Trying a broader compatibility image instead. "
                        f"Tip: `{login_hint(token_source, gh_user)}` using a GitHub token can unlock access.{scope_note}{extra}"
                    )
            if fallback:
                run_command(["docker", "pull", fallback], dry_run=dry_run, capture_output=True)
                success(f"fallback image ready: {label} -> {fallback}")
                return fallback
        hint = (
            "If the image is private or rate-limited, run `docker login ghcr.io` with a GitHub token "
            "or set an override via --image label=image."
        )
        raise ActError(f"{exc} {hint}") from exc


TS_PATH = shutil.which("ts")
ANSI_RE = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")
TAIL_RE = re.compile(r"^\[(?P<id>[^\]]+)\]\s+(?P<body>.*)")


def run_command(
        cmd: List[str],
        *,
        env: Optional[Dict[str, str]] = None,
        dry_run: bool = False,
        capture_output: bool = False,
        tail_lines: int = 0,
        log_file: Optional[Path] = None,
        log_dir: Optional[Path] = None,
) -> None:
    pretty = " ".join(shlex.quote(str(part)) for part in cmd)
    ui.command(f"$ {pretty}", icon="🧰 ")
    if dry_run:
        return

    if capture_output:
        result = subprocess.run(
            cmd,
            env=env,
            cwd=str(ROOT),
            check=False,
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            stdout = result.stdout.strip() if result.stdout else ""
            stderr = result.stderr.strip() if result.stderr else ""
            details = "\n".join(part for part in (stdout, stderr) if part)
            suffix = f"\n{details}" if details else ""
            raise ActError(f"Command failed with exit code {result.returncode}: {pretty}{suffix}")
        return

    # Stream output with timestamps; use ts if available, otherwise manual prefix.
    render_tail = tail_lines > 0 and sys.stdout.isatty()
    multi_tail = tail_lines > 0
    displayed_tail_lines = 0
    displayed_tail_height = 0
    if multi_tail:
        tails: "OrderedDict[str, deque[str]]" = OrderedDict()
        tail_log_paths: Dict[str, Path] = {}
    else:
        tail = deque(maxlen=tail_lines) if tail_lines > 0 else None

    log_handle = None
    log_handles: Dict[str, TextIO] = {}
    if log_file:
        log_file.parent.mkdir(parents=True, exist_ok=True)
        log_handle = open(log_file, "w", encoding="utf-8")
    if log_dir:
        log_dir.mkdir(parents=True, exist_ok=True)

    proc = subprocess.Popen(
        cmd,
        env=env,
        cwd=str(ROOT),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    assert proc.stdout is not None

    if TS_PATH:
        ts_proc = subprocess.Popen(
            [TS_PATH, "[%H:%M:%S]"],
            stdin=proc.stdout,
            stdout=subprocess.PIPE,
            text=True,
        )
        proc.stdout.close()
        stream = ts_proc.stdout
    else:
        ts_proc = None
        stream = proc.stdout

    def _render_tail():
        nonlocal displayed_tail_lines, displayed_tail_height
        if (not multi_tail and tail is None) or (multi_tail and not tails) or not render_tail:
            return
        width = max(1, shutil.get_terminal_size(fallback=(80, 20)).columns)
        def _sep(label: str = "", path_text: str = "") -> str:
            parts = []
            if label:
                parts.append(f"[{label}]")
            if path_text:
                parts.append(path_text)
            raw = " ".join(parts)
            prefix = f"{raw} " if raw else ""
            filler = "=" * max(0, width - len(prefix))
            line = prefix + filler
            if ui.color_enabled:
                return f"{TextUI.COLOR['section']}{line}{TextUI.COLOR['reset']}"
            return line
        if multi_tail:
            lines_to_render: List[str] = []
            for index, (tail_id, q) in enumerate(tails.items()):
                rel_path = ""
                if log_dir:
                    tail_path = tail_log_paths.get(tail_id)
                    if tail_path:
                        try:
                            rel_path = str(Path(tail_path).relative_to(ROOT))
                        except ValueError:
                            rel_path = str(tail_path)
                lines_to_render.append(_sep(tail_id, rel_path))
                lines_to_render.extend(list(q)[-3:])
                is_last = index == (len(tails) - 1)
                if is_last:
                    lines_to_render.append(_sep())
        else:
            sep_line = _sep()
            lines_to_render = [sep_line, *tail, sep_line]
        wrapped_height = 0
        for line in lines_to_render:
            visible = ANSI_RE.sub("", line)
            lines_needed = max(1, math.ceil(len(visible) / width))
            wrapped_height += lines_needed
        wrapped_lines = lines_to_render
        # Move cursor up to start of the tail block, clear, and redraw.
        if displayed_tail_height:
            sys.stdout.write(f"\x1b[{displayed_tail_height}F")  # move up
        for _ in range(displayed_tail_height):
            sys.stdout.write("\x1b[2K\x1b[1E")  # clear line, move down
        if displayed_tail_height:
            sys.stdout.write(f"\x1b[{displayed_tail_height}F")  # move back up
        for chunk in wrapped_lines:
            sys.stdout.write("\x1b[2K" + chunk + "\n")
        sys.stdout.flush()
        if multi_tail:
            displayed_tail_lines = sum(min(len(q), 3) for q in tails.values())
        else:
            displayed_tail_lines = len(tail)
        displayed_tail_height = wrapped_height

    if stream:
        for line in stream:
            line = line.rstrip("\n")
            parsed_id = "default"
            parsed_body = line
            match = TAIL_RE.match(line)
            if match:
                parsed_id = match.group("id").strip()
                parsed_body = match.group("body").strip()
            display_line = parsed_body
            if log_dir:
                handle = log_handles.get(parsed_id)
                if handle is None:
                    path = log_dir / f"{parsed_id}.txt"
                    path.parent.mkdir(parents=True, exist_ok=True)
                    handle = open(path, "w", encoding="utf-8")
                    log_handles[parsed_id] = handle
                    tail_log_paths[parsed_id] = path
                handle.write(parsed_body + "\n")
            elif log_handle:
                log_handle.write(line + "\n")

            if multi_tail:
                if parsed_id not in tails:
                    tails[parsed_id] = deque(maxlen=tail_lines)
                tails[parsed_id].append(display_line)
                if render_tail:
                    _render_tail()
                elif TS_PATH:
                    ui.info(display_line, icon="")
                else:
                    timestamp = datetime.now().strftime("%H:%M:%S")
                    print(f"{timestamp} │ {display_line}")
            elif tail is not None:
                tail.append(line)
                if render_tail:
                    _render_tail()
                elif TS_PATH:
                    ui.info(line, icon="")
                else:
                    timestamp = datetime.now().strftime("%H:%M:%S")
                    print(f"{timestamp} │ {line}")
            elif TS_PATH:
                ui.info(line, icon="")
            else:
                timestamp = datetime.now().strftime("%H:%M:%S")
                print(f"{timestamp} │ {line}")

    proc.wait()
    ts_rc = 0
    if ts_proc:
        ts_proc.wait()
        ts_rc = ts_proc.returncode or 0

    if log_handle:
        log_handle.flush()
        log_handle.close()

    def _format_tail(lines: deque[str]) -> str:
        if not lines:
            return ""
        return "\n".join(lines)

    if proc.returncode != 0 or ts_rc != 0:
        tail_note = ""
        if multi_tail and tails:
            rendered = []
            for tid, q in tails.items():
                rendered.append(f"[{tid}]")
                rendered.extend(list(q))
            tail_note = f"\nLast lines:\n" + "\n".join(rendered)
        elif not multi_tail and tail:
            tail_note = f"\nLast {len(tail)} line(s):\n{_format_tail(tail)}"
        log_note = ""
        if log_dir:
            log_note = f"\nLogs: {log_dir}"
        elif log_file:
            log_note = f"\nLog: {log_file}"
        raise ActError(f"Command failed with exit code {proc.returncode or ts_rc}: {pretty}{tail_note}{log_note}")

    if multi_tail and tails:
        ui.section_small("Last lines", icon="🧾 ")
        for tid, q in tails.items():
            ui.section_small(f"[{tid}]", icon="")
            ui.info(_format_tail(deque(list(q)[-3:])))
        if log_dir:
            ui.kv("Logs", str(log_dir), icon="🗂️ ")
    elif tail:
        ui.section_small(f"Last {len(tail)} line(s)", icon="🧾 ")
        ui.info(_format_tail(tail))
        if log_file:
            ui.kv("Full log", str(log_file), icon="🗂️ ")


def run_act_job(
        act_path: Path,
        job: str,
        workflow: Path,
        platform_images: Dict[str, str],
        container_arch: str,
        secrets_file: Path,
        env: Dict[str, str],
        dry_run: bool,
        reuse: bool,
) -> None:
    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    log_dir = LOG_DIR / f"{job}-{timestamp}"
    secret_args: List[str] = []
    for key in ("GITHUB_TOKEN", "GH_TOKEN"):
        val = env.get(key, "")
        if val and not is_placeholder_token(val):
            secret_args.extend(["-s", f"{key}={val}"])
    cmd = [
        str(act_path),
        "pull_request",
        "-W",
        str(workflow),
        "-j",
        job,
        "--container-architecture",
        container_arch,
        "--secret-file",
        str(secrets_file),
    ]
    cmd.extend(secret_args)
    if reuse:
        cmd.append("--reuse")
    for label, image in platform_images.items():
        cmd.extend(["-P", f"{label}={image}"])
    run_command(cmd, env=env, dry_run=dry_run, tail_lines=5, log_dir=log_dir)


def build_env() -> Dict[str, str]:
    env = os.environ.copy()
    env.setdefault("ACT_CACHE_DIR", str(CACHE_DIR))
    env.setdefault("ACT_WORKDIR", str(ROOT))
    env.setdefault("ACT_RUNNER_TEMP", str(TMP_DIR))
    env.setdefault("GITHUB_TOKEN", DEFAULT_TOKEN)
    env.setdefault("GH_TOKEN", DEFAULT_TOKEN)
    return env


def _gh_cli_token() -> Optional[str]:
    """Try to source a token from the GitHub CLI if available."""
    gh = shutil.which("gh")
    if not gh:
        return None

    for cmd in ([gh, "auth", "token"], [gh, "auth", "status", "--show-token"]):
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        except Exception:
            continue
        token = result.stdout.strip()
        if token:
            return token
    return None


def gh_username() -> Optional[str]:
    gh = shutil.which("gh")
    if not gh:
        return None
    try:
        result = subprocess.run(
            [gh, "api", "user", "-q", ".login"],
            capture_output=True,
            text=True,
            check=True,
        )
        name = result.stdout.strip()
        return name or None
    except Exception:
        return None


def gh_cli_scopes() -> Optional[List[str]]:
    gh = shutil.which("gh")
    if not gh:
        return None
    try:
        result = subprocess.run(
            [gh, "auth", "status", "-t"],
            capture_output=True,
            text=True,
            check=True,
        )
    except Exception:
        return None
    for line in result.stdout.splitlines():
        if "scopes:" in line.lower():
            _, rhs = line.split(":", 1)
            scopes = [scope.strip() for scope in rhs.split(",") if scope.strip()]
            return scopes or None
    return None


def _parse_secrets_tokens(path: Path) -> Dict[str, str]:
    values: Dict[str, str] = {}
    if not path.exists():
        return values
    for line in path.read_text(encoding="utf-8").splitlines():
        if "=" not in line or line.strip().startswith("#"):
            continue
        key, val = line.split("=", 1)
        values[key.strip()] = val.strip()
    return values


def hydrate_token(env: Dict[str, str], secrets_file: Path) -> Optional[str]:
    """Populate GH_TOKEN/GITHUB_TOKEN and return a string describing the source."""
    # Prefer explicit env overrides
    if not is_placeholder_token(env.get("GH_TOKEN", "")):
        env["GITHUB_TOKEN"] = env["GH_TOKEN"]
        log("Using token from GH_TOKEN environment variable.", emoji="🔑", color="green")
        return "env GH_TOKEN"
    if not is_placeholder_token(env.get("GITHUB_TOKEN", "")):
        env["GH_TOKEN"] = env["GITHUB_TOKEN"]
        log("Using token from GITHUB_TOKEN environment variable.", emoji="🔑", color="green")
        return "env GITHUB_TOKEN"

    # Next, secrets file
    secrets = _parse_secrets_tokens(secrets_file)
    for key in ("GH_TOKEN", "GITHUB_TOKEN"):
        val = secrets.get(key, "")
        if val and not is_placeholder_token(val):
            env["GH_TOKEN"] = env["GITHUB_TOKEN"] = val
            log(f"Using token from {secrets_file} ({key}).", emoji="🔑", color="green")
            return f"{secrets_file}::{key}"

    # Finally, GitHub CLI
    token = _gh_cli_token()
    if token and not is_placeholder_token(token):
        env["GH_TOKEN"] = env["GITHUB_TOKEN"] = token
        log("Using token from GitHub CLI (gh auth).", emoji="🔑", color="green")
        return "GitHub CLI (gh auth token)"

    warn(
        "Using placeholder tokens. Set GH_TOKEN/GITHUB_TOKEN to a real GitHub token "
        "in your environment or edit local/act/secrets.env for reliable action and clone access. "
        "Create one at https://github.com/settings/tokens?type=beta (public_repo scope is enough for public repos)."
    )
    return None


def login_hint(token_source: Optional[str], gh_user: Optional[str]) -> str:
    user = gh_user or "<github-username>"
    default = f'echo "<github-token>" | docker login ghcr.io -u {user} --password-stdin'
    if not token_source:
        return default
    if token_source.startswith("env GH_TOKEN"):
        return f'echo "$GH_TOKEN" | docker login ghcr.io -u {user} --password-stdin'
    if token_source.startswith("env GITHUB_TOKEN"):
        return f'echo "$GITHUB_TOKEN" | docker login ghcr.io -u {user} --password-stdin'
    if token_source.startswith("GitHub CLI"):
        return f'gh auth token | docker login ghcr.io -u {user} --password-stdin'
    if "::" in token_source and "secrets" in token_source:
        return (
            f'grep -m1 "GH_TOKEN=" local/act/secrets.env | cut -d= -f2 | '
            f'docker login ghcr.io -u {user} --password-stdin'
        )
    return default


def token_retrieve_hint(token_source: Optional[str]) -> str:
    if not token_source:
        return (
            "No real token found. Set GH_TOKEN/GITHUB_TOKEN in your shell or in local/act/secrets.env. "
            "Create one at https://github.com/settings/tokens?type=beta (public_repo scope works for public repos)."
        )
    if token_source.startswith("env GH_TOKEN"):
        return "Token source: GH_TOKEN environment variable (check with `echo \"$GH_TOKEN\"`)."
    if token_source.startswith("env GITHUB_TOKEN"):
        return "Token source: GITHUB_TOKEN environment variable (check with `echo \"$GITHUB_TOKEN\"`)."
    if token_source.startswith("GitHub CLI"):
        return "Token source: GitHub CLI; view it with `gh auth token`."
    if "::" in token_source and "secrets" in token_source:
        return "Token source: local/act/secrets.env; open that file to copy GH_TOKEN/GITHUB_TOKEN."
    return "Token source unknown; ensure GH_TOKEN/GITHUB_TOKEN are set to a valid PAT."


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--act-path",
        help="Use an explicit act binary instead of auto-installing.",
    )
    parser.add_argument(
        "--act-version",
        help="Act release tag to install when a local copy is required (default: latest).",
    )
    parser.add_argument(
        "--jobs",
        nargs="+",
        help="Subset of job IDs to run. Defaults to every job listed in ci.yml.",
    )
    parser.add_argument(
        "--max-jobs",
        type=int,
        default=None,
        help="Run at most this many jobs (useful for quick repros).",
    )
    parser.add_argument(
        "--image",
        action="append",
        default=[],
        help="Override runner image mapping (label=image). Can be supplied multiple times.",
    )
    parser.add_argument(
        "--env",
        action="append",
        default=[],
        help="Additional environment variables to pass to act (KEY=VALUE).",
    )
    parser.add_argument(
        "--list-jobs",
        action="store_true",
        help="List the discovered job IDs and exit.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the act commands without executing them.",
    )
    parser.add_argument(
        "--reuse",
        action="store_true",
        help="Pass --reuse to act so successive jobs share containers.",
    )
    parser.add_argument(
        "--clean-cache",
        action="store_true",
        help="Prune Docker images/containers before running (docker system prune -af).",
    )
    return parser.parse_args()


def main() -> int:
    ui.section_big("Run CI locally with act", icon="🧪")
    args = parse_args()
    ensure_directories()
    token_source: Optional[str] = None
    gh_user: Optional[str] = None

    try:
        if args.clean_cache:
            run_command(["docker", "system", "prune", "-af"], dry_run=args.dry_run)
        jobs = parse_job_ids(WORKFLOW_FILE)
        if args.list_jobs:
            ui.section_small("Workflow jobs", icon="📋")
            print("\n".join(jobs))
            return 0
        ui.section_big("Environment", icon="🧭")
        check_docker()
        system, machine_arch = detect_platform()
        ui.kv("Host", f"{system} / {machine_arch}", icon="💻 ")
        container_arch = resolve_container_arch(machine_arch)
        ui.kv("Container architecture", container_arch, icon="📦 ")
        act_binary = find_act(args.act_path, args.act_version, allow_install=not args.dry_run)
        ui.kv("Using act", str(act_binary), icon="🧭 ")
        secrets_file = ensure_secrets_file(DEFAULT_SECRETS_FILE)
        ui.kv("Secrets file", str(secrets_file), icon="🔐 ")
        ui.kv("Workflow file", str(WORKFLOW_FILE), icon="🗂️ ")
        defaults = select_default_images(container_arch)
        platform_images = build_platform_overrides(args.image, defaults)
        ui.section_big("Workflow metadata", icon="🗂️")
        ui.kv("Runner image map", ", ".join(f"{k}={v}" for k, v in platform_images.items()), icon="🖼️ ")
        runs_on_labels = set(parse_runs_on_labels(WORKFLOW_FILE))
        needed_labels = set(platform_images.keys()) if args.list_jobs else {lbl for lbl in runs_on_labels if lbl in platform_images}
        if needed_labels:
            ui.kv("Labels to pre-pull", ", ".join(sorted(needed_labels)), icon="🎯 ")
        targets = args.jobs or jobs
        if args.max_jobs is not None:
            targets = targets[: args.max_jobs]
        missing = [job for job in targets if job not in jobs]
        if missing:
            raise ActError(f"Unknown job(s): {', '.join(missing)}")
        ui.section_big("Authentication", icon="🔑")
        env = build_env()
        for kv in args.env:
            if "=" in kv:
                key, val = kv.split("=", 1)
                env[key] = val
            else:
                warn(f"Ignoring --env entry without '=': {kv}")
        token_source = hydrate_token(env, secrets_file)
        gh_user = gh_username()
        gh_scopes = gh_cli_scopes() if token_source and token_source.startswith("GitHub CLI") else None
        # For jobs that use dynamic matrix labels, act will pull on demand; we pre-pull only static runs-on labels.
        platform_images = build_platform_overrides(args.image, defaults)

        if needed_labels:
            ui.section_big("Preparing runner images", icon="📥")
        for label in sorted(needed_labels):
            image = platform_images[label]
            platform_images[label] = docker_pull(
                image,
                args.dry_run,
                label,
                token_source,
                gh_user,
                gh_scopes,
                container_arch,
            )

        ui.section_big("Executing jobs", icon="🚀")
        for job in targets:
            log(f"running job '{job}'", emoji="🚀", color="green")
            run_act_job(
                act_binary,
                job,
                WORKFLOW_FILE,
                platform_images,
                container_arch,
                secrets_file,
                env,
                args.dry_run,
                args.reuse,
            )
        ui.section_big("Summary", icon="✅")
        ui.ok("All requested jobs completed via act.")
    except ActError as exc:
        msg = str(exc)
        error(msg)
        lower = msg.lower()
        if "authentication required" in lower or "invalid username or token" in lower:
            warn(
                "GitHub token likely missing/placeholder. Set GH_TOKEN/GITHUB_TOKEN as a "
                "real PAT (read-only is fine) or edit local/act/secrets.env, then rerun. "
                "Create one at https://github.com/settings/tokens?type=beta (public_repo is enough for public repos)."
            )
            warn(token_retrieve_hint(token_source))
            warn(f"If pulling from ghcr.io, try: {login_hint(token_source, gh_user)}")
        return 1
    except KeyboardInterrupt:
        warn("interrupted")
        return 130
    return 0


if __name__ == "__main__":
    sys.exit(main())
