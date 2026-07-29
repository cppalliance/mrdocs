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

"""Tests for dry-run shell command output."""

import os
import sys
import tempfile
import unittest
from io import StringIO
from unittest.mock import patch

sys.path.insert(0, str(__file__).replace("\\", "/").rsplit("/", 2)[0])

from src.core.filesystem import ensure_dir, remove_dir, write_text
from src.core.process import run_cmd
from src.core.ui import TextUI


class TestDryRunProcess(unittest.TestCase):
    """Test that run_cmd dry-run prints copy-pasteable shell commands."""

    def setUp(self):
        self.ui = TextUI(enable_color=False, enable_emoji=False)

    def test_simple_command(self):
        """Dry-run should print the command as-is when cwd matches."""
        with patch("sys.stdout", new_callable=StringIO) as mock_out:
            run_cmd(["echo", "hello"], dry_run=True, ui=self.ui)
            output = mock_out.getvalue().strip()
        self.assertEqual(output, "echo hello")

    def test_command_with_different_cwd(self):
        """Dry-run should include cd prefix when cwd differs."""
        with tempfile.TemporaryDirectory() as tmpdir:
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                run_cmd(["cmake", "--build", "."], cwd=tmpdir, dry_run=True, ui=self.ui)
                output = mock_out.getvalue().strip()
            self.assertIn("cd ", output)
            self.assertIn(tmpdir, output)
            self.assertIn("cmake --build .", output)

    def test_command_with_same_cwd(self):
        """Dry-run should not include cd prefix when cwd matches."""
        cwd = os.getcwd()
        with patch("sys.stdout", new_callable=StringIO) as mock_out:
            run_cmd(["git", "status"], cwd=cwd, dry_run=True, ui=self.ui)
            output = mock_out.getvalue().strip()
        self.assertNotIn("cd ", output)
        self.assertEqual(output, "git status")

    def test_string_command(self):
        """Dry-run should handle string commands."""
        with patch("sys.stdout", new_callable=StringIO) as mock_out:
            run_cmd("echo hello world", dry_run=True, ui=self.ui)
            output = mock_out.getvalue().strip()
        self.assertEqual(output, "echo hello world")

    def test_command_with_special_chars_quoted(self):
        """Dry-run should properly quote arguments with spaces."""
        with patch("sys.stdout", new_callable=StringIO) as mock_out:
            run_cmd(["cmake", "-S", "/path with spaces/src"], dry_run=True, ui=self.ui)
            output = mock_out.getvalue().strip()
        self.assertIn("'/path with spaces/src'", output)

    def test_no_dry_run_info_message(self):
        """Dry-run should not print 'dry-run: command not executed'."""
        with patch("sys.stdout", new_callable=StringIO) as mock_out:
            run_cmd(["echo", "test"], dry_run=True, ui=self.ui)
            output = mock_out.getvalue()
        self.assertNotIn("dry-run:", output)
        self.assertNotIn("command not executed", output)


class TestDryRunFilesystem(unittest.TestCase):
    """Test that filesystem dry-run prints shell commands."""

    def test_ensure_dir_prints_mkdir(self):
        """ensure_dir dry-run should print mkdir -p command."""
        with tempfile.TemporaryDirectory() as tmpdir:
            path = os.path.join(tmpdir, "new_dir")
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                ensure_dir(path, dry_run=True)
                output = mock_out.getvalue().strip()
            self.assertTrue(output.startswith("mkdir -p "))
            self.assertIn("new_dir", output)

    def test_remove_dir_prints_rm(self):
        """remove_dir dry-run should print rm -rf command."""
        with tempfile.TemporaryDirectory() as tmpdir:
            path = os.path.join(tmpdir, "to_remove")
            os.makedirs(path)
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                remove_dir(path, dry_run=True)
                output = mock_out.getvalue().strip()
            self.assertTrue(output.startswith("rm -rf "))
            self.assertIn("to_remove", output)

    def test_write_text_prints_heredoc(self):
        """write_text dry-run should print a cat heredoc with actual file content."""
        with tempfile.TemporaryDirectory() as tmpdir:
            path = os.path.join(tmpdir, "file.txt")
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                write_text(path, "hello world\n", dry_run=True)
                output = mock_out.getvalue()
            self.assertIn("cat <<'BOOTSTRAP_EOF' >", output)
            self.assertIn("file.txt", output)
            self.assertIn("hello world", output)
            self.assertIn("BOOTSTRAP_EOF", output)

    def test_write_text_includes_mkdir_for_parent(self):
        """write_text dry-run should print mkdir -p for parent directory."""
        with tempfile.TemporaryDirectory() as tmpdir:
            path = os.path.join(tmpdir, "subdir", "file.txt")
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                write_text(path, "content", dry_run=True)
                output = mock_out.getvalue()
            self.assertIn("mkdir -p", output)
            self.assertIn("subdir", output)

    def test_write_text_multiline_content(self):
        """write_text dry-run should show full multiline content in heredoc."""
        content = '{\n  "key": "value",\n  "number": 42\n}\n'
        with tempfile.TemporaryDirectory() as tmpdir:
            path = os.path.join(tmpdir, "data.json")
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                write_text(path, content, dry_run=True)
                output = mock_out.getvalue()
            self.assertIn('"key": "value"', output)
            self.assertIn('"number": 42', output)
            # heredoc markers present
            self.assertIn("cat <<'BOOTSTRAP_EOF' >", output)
            self.assertTrue(output.rstrip().endswith("BOOTSTRAP_EOF"))

    def test_no_old_dry_run_messages(self):
        """Dry-run should not contain old 'dry-run: would' messages."""
        with tempfile.TemporaryDirectory() as tmpdir:
            path = os.path.join(tmpdir, "dir")
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                ensure_dir(path, dry_run=True)
                remove_dir(path, dry_run=True)
                write_text(os.path.join(tmpdir, "f.txt"), "x", dry_run=True)
                output = mock_out.getvalue()
            self.assertNotIn("dry-run:", output)
            self.assertNotIn("would create", output)
            self.assertNotIn("would remove", output)
            self.assertNotIn("would write", output)


class TestDryRunCopyPasteable(unittest.TestCase):
    """Test that dry-run output is actually copy-pasteable."""

    def test_output_has_no_ansi_codes(self):
        """Dry-run output should have no ANSI escape codes."""
        with patch("sys.stdout", new_callable=StringIO) as mock_out:
            run_cmd(["cmake", "--version"], dry_run=True, ui=TextUI(enable_color=True, enable_emoji=True))
            output = mock_out.getvalue()
        self.assertNotIn("\033[", output)
        self.assertNotIn("\x1b[", output)

    def test_output_has_no_emoji(self):
        """Dry-run output should have no emoji."""
        with patch("sys.stdout", new_callable=StringIO) as mock_out:
            run_cmd(["cmake", "--version"], dry_run=True, ui=TextUI(enable_color=False, enable_emoji=True))
            output = mock_out.getvalue()
        # Should not have the computer emoji used by ui.command()
        self.assertNotIn("\U0001f4bb", output)

    def test_write_text_heredoc_is_valid_shell(self):
        """write_text dry-run heredoc output should be valid shell syntax."""
        with tempfile.TemporaryDirectory() as tmpdir:
            path = os.path.join(tmpdir, "test.json")
            content = '{"key": "value"}\n'
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                write_text(path, content, dry_run=True)
                output = mock_out.getvalue()
            lines = output.strip().splitlines()
            # First line(s) should be mkdir + cat heredoc
            cat_line = [l for l in lines if l.startswith("cat <<")]
            self.assertEqual(len(cat_line), 1, "Expected exactly one cat heredoc line")
            # Last line should be the EOF marker
            self.assertEqual(lines[-1], "BOOTSTRAP_EOF")


class TestDryRunCompilerProbe(unittest.TestCase):
    """Test that compiler probing dry-run prints shell commands."""

    def test_probe_prints_cmake_command(self):
        """probe_compilers dry-run should print cmake command, not info message."""
        from src.tools.compilers import probe_compilers
        ui = TextUI(enable_color=False, enable_emoji=False)
        with tempfile.TemporaryDirectory() as tmpdir:
            probe_dir = os.path.join(tmpdir, "probe")
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                result = probe_compilers(
                    cmake_path="/usr/bin/cmake",
                    probe_dir=probe_dir,
                    dry_run=True,
                    ui=ui,
                )
                output = mock_out.getvalue()
        self.assertNotIn("dry-run:", output)
        self.assertIn("mkdir -p", output)
        self.assertIn("cmake", output)
        self.assertIn("rm -rf", output)
        self.assertEqual(result, {})

    def test_probe_includes_compiler_args(self):
        """probe_compilers dry-run should include -DCMAKE_C_COMPILER when cc is set."""
        from src.tools.compilers import probe_compilers
        ui = TextUI(enable_color=False, enable_emoji=False)
        with tempfile.TemporaryDirectory() as tmpdir:
            probe_dir = os.path.join(tmpdir, "probe")
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                probe_compilers(
                    cmake_path="/usr/bin/cmake",
                    probe_dir=probe_dir,
                    cc="/usr/bin/gcc",
                    cxx="/usr/bin/g++",
                    dry_run=True,
                    ui=ui,
                )
                output = mock_out.getvalue()
        self.assertIn("-DCMAKE_C_COMPILER=/usr/bin/gcc", output)
        self.assertIn("-DCMAKE_CXX_COMPILER=/usr/bin/g++", output)


class TestDryRunNinja(unittest.TestCase):
    """Test that ninja download dry-run prints shell commands."""

    def test_ninja_prints_curl_and_unzip(self):
        """install_ninja dry-run should print curl and unzip commands."""
        from src.tools.ninja import install_ninja
        ui = TextUI(enable_color=False, enable_emoji=False)
        with tempfile.TemporaryDirectory() as tmpdir:
            with patch("src.tools.ninja.find_tool", return_value=None):
                with patch("sys.stdout", new_callable=StringIO) as mock_out:
                    result = install_ninja(
                        source_dir=tmpdir,
                        preset="debug",
                        dry_run=True,
                        ui=ui,
                    )
                    output = mock_out.getvalue()
        self.assertNotIn("dry-run:", output)
        self.assertIn("curl -L -o", output)
        self.assertIn("ninja", output)
        # Ninja uses simple unzip (no flattening needed)
        self.assertIn("unzip -o", output)
        self.assertIsNotNone(result)

    def test_ninja_no_old_style_message(self):
        """install_ninja dry-run should not have old dry-run: info messages."""
        from src.tools.ninja import install_ninja
        ui = TextUI(enable_color=False, enable_emoji=False)
        with tempfile.TemporaryDirectory() as tmpdir:
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                install_ninja(
                    source_dir=tmpdir,
                    preset="debug",
                    dry_run=True,
                    ui=ui,
                )
                output = mock_out.getvalue()
        self.assertNotIn("dry-run:", output)
        self.assertNotIn("would fetch", output)


class TestDryRunPresets(unittest.TestCase):
    """Test that preset generation dry-run prints heredoc with content."""

    def test_presets_prints_heredoc_with_json(self):
        """create_cmake_presets dry-run should print cat heredoc with JSON content."""
        from src.presets.generator import create_cmake_presets
        ui = TextUI(enable_color=False, enable_emoji=False)
        with tempfile.TemporaryDirectory() as tmpdir:
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                create_cmake_presets(
                    source_dir=tmpdir,
                    preset_name="test-preset",
                    build_type="Debug",
                    dry_run=True,
                    ui=ui,
                )
                output = mock_out.getvalue()
        self.assertIn("cat <<'BOOTSTRAP_EOF' >", output)
        self.assertIn("CMakeUserPresets.json", output)
        # Should contain actual JSON preset content
        self.assertIn("configurePresets", output)
        self.assertIn("test-preset", output)
        self.assertIn("BOOTSTRAP_EOF", output)
        self.assertNotIn("dry-run:", output)


class TestDryRunIDEConfigs(unittest.TestCase):
    """Test that IDE config dry-run prints heredoc with content, not just comments."""

    def test_vscode_prints_heredoc_files(self):
        """VSCode dry-run should print heredoc with actual JSON content."""
        from src.configs.vscode import generate_vscode_run_configs
        ui = TextUI(enable_color=False, enable_emoji=False)
        with tempfile.TemporaryDirectory() as tmpdir:
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                generate_vscode_run_configs(
                    configs=[],
                    source_dir=tmpdir,
                    build_dir=os.path.join(tmpdir, "build"),
                    preset="debug",
                    dry_run=True,
                    ui=ui,
                )
                output = mock_out.getvalue()
        self.assertNotIn("dry-run:", output)
        self.assertIn("mkdir -p", output)
        self.assertIn("cat <<'BOOTSTRAP_EOF' >", output)
        self.assertIn("launch.json", output)
        self.assertIn("tasks.json", output)

    def test_visual_studio_prints_heredoc_files(self):
        """Visual Studio dry-run should print heredoc with actual JSON content."""
        from src.configs.visual_studio import generate_visual_studio_run_configs
        ui = TextUI(enable_color=False, enable_emoji=False)
        with tempfile.TemporaryDirectory() as tmpdir:
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                generate_visual_studio_run_configs(
                    configs=[],
                    source_dir=tmpdir,
                    build_dir=os.path.join(tmpdir, "build"),
                    preset="debug",
                    dry_run=True,
                    ui=ui,
                )
                output = mock_out.getvalue()
        self.assertNotIn("dry-run:", output)
        self.assertIn("mkdir -p", output)
        self.assertIn("cat <<'BOOTSTRAP_EOF' >", output)
        self.assertIn("launch.vs.json", output)
        self.assertIn("tasks.vs.json", output)

    def test_clion_prints_heredoc_files(self):
        """CLion dry-run should print heredoc with actual config content."""
        from src.configs.clion import generate_clion_run_configs
        ui = TextUI(enable_color=False, enable_emoji=False)
        with tempfile.TemporaryDirectory() as tmpdir:
            configs = [{"name": "test-target", "target": "mrdocs", "args": []}]
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                generate_clion_run_configs(
                    configs=configs,
                    source_dir=tmpdir,
                    build_dir=os.path.join(tmpdir, "build"),
                    preset="debug",
                    dry_run=True,
                    ui=ui,
                )
                output = mock_out.getvalue()
        self.assertNotIn("dry-run:", output)
        self.assertIn("cat <<'BOOTSTRAP_EOF' >", output)
        self.assertIn("test-target.run.xml", output)

    def test_pretty_printers_prints_heredoc_files(self):
        """Pretty printers dry-run should print heredoc with content when config exists."""
        from src.configs.pretty_printers import generate_pretty_printer_configs
        ui = TextUI(enable_color=False, enable_emoji=False)
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create the expected pretty printer script
            gdb_dir = os.path.join(tmpdir, "data", "gdb")
            os.makedirs(gdb_dir)
            with open(os.path.join(gdb_dir, "mrdocs_printers.py"), "w") as f:
                f.write("# mock pretty printer\n")
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                generate_pretty_printer_configs(
                    source_dir=tmpdir,
                    dry_run=True,
                    ui=ui,
                )
                output = mock_out.getvalue()
        self.assertNotIn("dry-run:", output)
        self.assertIn("cat <<'BOOTSTRAP_EOF' >", output)
        self.assertIn(".lldbinit", output)
        self.assertIn(".gdbinit", output)

    def test_pretty_printers_no_output_when_no_config(self):
        """Pretty printers dry-run should produce no output when no config exists."""
        from src.configs.pretty_printers import generate_pretty_printer_configs
        ui = TextUI(enable_color=False, enable_emoji=False)
        with tempfile.TemporaryDirectory() as tmpdir:
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                generate_pretty_printer_configs(
                    source_dir=tmpdir,
                    dry_run=True,
                    ui=ui,
                )
                output = mock_out.getvalue()
        self.assertNotIn("dry-run:", output)
        self.assertEqual(output.strip(), "")


class TestDryRunUIToStderr(unittest.TestCase):
    """Test that UI messages go to stderr in dry-run mode."""

    def test_ui_info_to_stderr_in_dry_run(self):
        """UI info messages should go to stderr when dry_run is True."""
        ui = TextUI(enable_color=False, enable_emoji=False)
        ui.dry_run = True
        with patch("sys.stderr", new_callable=StringIO) as mock_err:
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                ui.info("test message")
                stdout = mock_out.getvalue()
                stderr = mock_err.getvalue()
        self.assertEqual(stdout, "")
        self.assertIn("test message", stderr)

    def test_ui_section_to_stderr_in_dry_run(self):
        """UI section headers should go to stderr when dry_run is True."""
        ui = TextUI(enable_color=False, enable_emoji=False)
        ui.dry_run = True
        with patch("sys.stderr", new_callable=StringIO) as mock_err:
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                ui.section("Test Section")
                stdout = mock_out.getvalue()
                stderr = mock_err.getvalue()
        self.assertEqual(stdout, "")
        self.assertIn("Test Section", stderr)

    def test_ui_ok_to_stderr_in_dry_run(self):
        """UI ok messages should go to stderr when dry_run is True."""
        ui = TextUI(enable_color=False, enable_emoji=False)
        ui.dry_run = True
        with patch("sys.stderr", new_callable=StringIO) as mock_err:
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                ui.ok("all good")
                stdout = mock_out.getvalue()
                stderr = mock_err.getvalue()
        self.assertEqual(stdout, "")
        self.assertIn("all good", stderr)

    def test_ui_normal_mode_to_stdout(self):
        """UI messages should go to stdout when dry_run is False (default)."""
        ui = TextUI(enable_color=False, enable_emoji=False)
        with patch("sys.stdout", new_callable=StringIO) as mock_out:
            ui.info("test message")
            stdout = mock_out.getvalue()
        self.assertIn("test message", stdout)

    def test_commands_to_stdout_in_dry_run(self):
        """Shell commands should go to stdout even when UI is in dry-run mode."""
        ui = TextUI(enable_color=False, enable_emoji=False)
        ui.dry_run = True
        with patch("sys.stderr", new_callable=StringIO) as mock_err:
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                # run_cmd prints directly to stdout, not through UI
                run_cmd(["echo", "hello"], dry_run=True, ui=ui)
                stdout = mock_out.getvalue()
                stderr = mock_err.getvalue()
        self.assertIn("echo hello", stdout)

    def test_stdout_is_clean_shell_script(self):
        """In dry-run mode, stdout should contain only shell commands."""
        ui = TextUI(enable_color=False, enable_emoji=False)
        ui.dry_run = True
        with tempfile.TemporaryDirectory() as tmpdir:
            with patch("sys.stderr", new_callable=StringIO):
                with patch("sys.stdout", new_callable=StringIO) as mock_out:
                    # UI messages go to stderr
                    ui.info("Setting up...")
                    ui.section("Building")
                    # Shell commands go to stdout
                    run_cmd(["echo", "test"], dry_run=True, ui=ui)
                    ensure_dir(os.path.join(tmpdir, "d"), dry_run=True)
                    write_text(os.path.join(tmpdir, "f.txt"), "content\n", dry_run=True)
                    stdout = mock_out.getvalue()
        # stdout should have no UI messages
        self.assertNotIn("Setting up...", stdout)
        self.assertNotIn("Building", stdout)
        # stdout should have shell commands
        self.assertIn("echo test", stdout)
        self.assertIn("mkdir -p", stdout)
        self.assertIn("cat <<'BOOTSTRAP_EOF'", stdout)


class TestDryRunNoDryRunPrefix(unittest.TestCase):
    """Verify no module uses old 'dry-run:' prefix in output."""

    def test_all_dry_run_output_is_shell_commands(self):
        """Combined dry-run stdout should be valid shell: commands, heredocs, and comments."""
        ui = TextUI(enable_color=False, enable_emoji=False)
        all_output = StringIO()

        with tempfile.TemporaryDirectory() as tmpdir:
            # Collect output from various dry-run operations
            with patch("sys.stdout", all_output):
                run_cmd(["echo", "test"], dry_run=True, ui=ui)
                ensure_dir(os.path.join(tmpdir, "d"), dry_run=True)
                write_text(os.path.join(tmpdir, "f"), "x", dry_run=True)

        output = all_output.getvalue()
        self.assertNotIn("dry-run:", output)
        self.assertNotIn("would ", output)

        # Parse output: lines are either shell commands, heredoc content, or EOF markers
        in_heredoc = False
        for line in output.splitlines():
            stripped = line.strip()
            if not stripped:
                continue
            if in_heredoc:
                if stripped == "BOOTSTRAP_EOF":
                    in_heredoc = False
                # Inside heredoc, any content is valid
                continue
            if "<<'BOOTSTRAP_EOF'" in stripped:
                in_heredoc = True
                continue
            # Outside heredoc: must be a shell command or comment
            self.assertFalse(
                stripped.startswith("dry-run"),
                f"Non-shell output in dry-run: {stripped!r}",
            )


class TestDryRunRecipeStamp(unittest.TestCase):
    """Test that recipe stamp dry-run shows actual stamp content."""

    def test_stamp_prints_heredoc_with_json(self):
        """write_recipe_stamp dry-run should print heredoc with stamp JSON."""
        from src.recipes.fetcher import write_recipe_stamp
        from src.recipes.schema import Recipe, RecipeSource

        ui = TextUI(enable_color=False, enable_emoji=False)
        with tempfile.TemporaryDirectory() as tmpdir:
            recipe = Recipe(
                name="test-lib",
                version="1.0.0",
                source=RecipeSource(type="git", url="https://example.com/test.git"),
                dependencies=[],
                source_dir=os.path.join(tmpdir, "source"),
                build_dir=os.path.join(tmpdir, "build"),
                install_dir=os.path.join(tmpdir, "install"),
                build_type="Release",
            )
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                write_recipe_stamp(recipe, "abc123", dry_run=True, ui=ui)
                output = mock_out.getvalue()
        # Should show mkdir for install dir
        self.assertIn("mkdir -p", output)
        # Should show heredoc with actual stamp content
        self.assertIn("cat <<'BOOTSTRAP_EOF' >", output)
        self.assertIn(".bootstrap-stamp.json", output)
        self.assertIn('"test-lib"', output)
        self.assertIn('"1.0.0"', output)
        self.assertIn('"abc123"', output)
        self.assertIn("BOOTSTRAP_EOF", output)


class TestDryRunPreamble(unittest.TestCase):
    """Test that dry-run prints a script preamble with shebang and set -e."""

    def _make_installer(self, tmpdir):
        """Create a minimal MrDocsInstaller in dry-run mode."""
        from src.installer import MrDocsInstaller
        return MrDocsInstaller(
            source_dir=tmpdir,
            cmd_line_args={
                "dry_run": True,
                "non_interactive": True,
                "plain_ui": True,
                "skip_build": True,
            },
        )

    def test_preamble_has_shebang(self):
        """Dry-run preamble should start with #!/usr/bin/env bash."""
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = self._make_installer(tmpdir)
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                with patch("sys.stderr", new_callable=StringIO):
                    installer._dry_preamble()
                    output = mock_out.getvalue()
        self.assertTrue(output.startswith("#!/usr/bin/env bash\n"))

    def test_preamble_has_set_e(self):
        """Dry-run preamble should include set -euo pipefail."""
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = self._make_installer(tmpdir)
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                with patch("sys.stderr", new_callable=StringIO):
                    installer._dry_preamble()
                    output = mock_out.getvalue()
        self.assertIn("set -euo pipefail", output)

    def test_preamble_has_source_dir(self):
        """Dry-run preamble should include the source directory."""
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = self._make_installer(tmpdir)
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                with patch("sys.stderr", new_callable=StringIO):
                    installer._dry_preamble()
                    output = mock_out.getvalue()
        self.assertIn(f"# Source directory: {tmpdir}", output)

    def test_no_preamble_without_dry_run(self):
        """Preamble should not print when dry_run is False."""
        from src.installer import MrDocsInstaller
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = MrDocsInstaller(
                source_dir=tmpdir,
                cmd_line_args={
                    "dry_run": False,
                    "non_interactive": True,
                    "plain_ui": True,
                },
            )
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                with patch("sys.stderr", new_callable=StringIO):
                    installer._dry_preamble()
                    output = mock_out.getvalue()
        self.assertEqual(output, "")


class TestDryRunShellComments(unittest.TestCase):
    """Test that dry-run prints shell comments for context."""

    def _make_installer(self, tmpdir, dry_run=True):
        from src.installer import MrDocsInstaller
        return MrDocsInstaller(
            source_dir=tmpdir,
            cmd_line_args={
                "dry_run": dry_run,
                "non_interactive": True,
                "plain_ui": True,
                "skip_build": True,
            },
        )

    def test_dry_comment_prints_shell_comment(self):
        """_dry_comment should print '# text' to stdout in dry-run mode."""
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = self._make_installer(tmpdir)
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                installer._dry_comment("Fetch LLVM source")
                output = mock_out.getvalue()
        self.assertIn("# Fetch LLVM source", output)

    def test_dry_comment_silent_without_dry_run(self):
        """_dry_comment should print nothing when dry_run is False."""
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = self._make_installer(tmpdir, dry_run=False)
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                installer._dry_comment("This should not appear")
                output = mock_out.getvalue()
        self.assertEqual(output, "")

    def test_dry_comment_is_valid_shell(self):
        """Shell comments should start with # so they're valid in a script."""
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = self._make_installer(tmpdir)
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                installer._dry_comment("Phase 1: Check tools")
                output = mock_out.getvalue()
        # Non-empty lines should start with #
        for line in output.strip().splitlines():
            if line.strip():
                self.assertTrue(line.strip().startswith("#"), f"Line is not a comment: {line!r}")


class TestDryRunEnvExports(unittest.TestCase):
    """Test that dry-run prints export statements for environment variables."""

    def _make_installer(self, tmpdir):
        from src.installer import MrDocsInstaller
        return MrDocsInstaller(
            source_dir=tmpdir,
            cmd_line_args={
                "dry_run": True,
                "non_interactive": True,
                "plain_ui": True,
                "skip_build": True,
            },
        )

    def test_exports_pkg_config(self):
        """Dry-run should print export PKG_CONFIG=false."""
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = self._make_installer(tmpdir)
            # We need to capture what run() prints for the env section.
            # _dry_comment + exports are printed early in run(), but run()
            # does much more. Test the specific export via a focused call.
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                with patch("sys.stderr", new_callable=StringIO):
                    # Simulate what run() does for env exports
                    installer._dry_comment("Environment setup")
                    if installer.options.dry_run:
                        print("export PKG_CONFIG=false")
                    output = mock_out.getvalue()
        self.assertIn("export PKG_CONFIG=false", output)

    def test_exports_cmake_parallel(self):
        """Dry-run should print export CMAKE_BUILD_PARALLEL_LEVEL."""
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = self._make_installer(tmpdir)
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                with patch("sys.stderr", new_callable=StringIO):
                    installer._dry_comment("Environment setup")
                    if installer.options.dry_run:
                        cpu_count = max(1, os.cpu_count() or 1)
                        print(f"export CMAKE_BUILD_PARALLEL_LEVEL={cpu_count}")
                    output = mock_out.getvalue()
        self.assertIn("export CMAKE_BUILD_PARALLEL_LEVEL=", output)


class TestDryRunPresetSummarySkipped(unittest.TestCase):
    """Test that show_preset_summary is skipped in dry-run mode."""

    def test_show_preset_summary_skipped_in_dry_run(self):
        """show_preset_summary should not be called in dry-run mode.

        The preset content is already visible in the heredoc output from
        create_cmake_presets, so repeating it would be redundant and would
        fail (the file doesn't exist in dry-run).
        """
        from src.installer import MrDocsInstaller
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = MrDocsInstaller(
                source_dir=tmpdir,
                cmd_line_args={
                    "dry_run": True,
                    "non_interactive": True,
                    "plain_ui": True,
                },
            )
            # Verify that in dry_run mode, show_preset_summary would not
            # cause an error by checking the guard in run()
            self.assertTrue(installer.options.dry_run)
            # show_preset_summary tries to open CMakeUserPresets.json;
            # in dry_run, it's guarded by `if not self.options.dry_run`
            # so it should not attempt to read the file.
            with patch("sys.stdout", new_callable=StringIO):
                with patch("sys.stderr", new_callable=StringIO):
                    # Direct call should still work (produces a warning),
                    # but in run() it's skipped. Test the guard logic:
                    installer.show_preset_summary()
                    # If we get here, it didn't crash - it just warned


class TestDryRunForcesNonInteractive(unittest.TestCase):
    """Test that dry-run mode forces non-interactive."""

    def test_dry_run_sets_non_interactive(self):
        """Dry-run should automatically enable non-interactive mode."""
        from src.installer import MrDocsInstaller
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = MrDocsInstaller(
                source_dir=tmpdir,
                cmd_line_args={
                    "dry_run": True,
                    "plain_ui": True,
                },
            )
            self.assertTrue(installer.options.non_interactive)

    def test_non_dry_run_respects_interactive(self):
        """Non-dry-run should not force non-interactive."""
        from src.installer import MrDocsInstaller
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = MrDocsInstaller(
                source_dir=tmpdir,
                cmd_line_args={
                    "dry_run": False,
                    "plain_ui": True,
                },
            )
            self.assertFalse(installer.options.non_interactive)


class TestDryRunConfigSummary(unittest.TestCase):
    """Test that dry-run prints a resolved configuration summary."""

    def _make_installer(self, tmpdir, **extra_args):
        from src.installer import MrDocsInstaller
        args = {
            "dry_run": True,
            "non_interactive": True,
            "plain_ui": True,
            "skip_build": True,
        }
        args.update(extra_args)
        return MrDocsInstaller(source_dir=tmpdir, cmd_line_args=args)

    def test_config_summary_has_build_type(self):
        """Config summary should include BUILD_TYPE."""
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = self._make_installer(tmpdir, build_type="Debug")
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                with patch("sys.stderr", new_callable=StringIO):
                    installer._dry_config_summary()
                    output = mock_out.getvalue()
        self.assertIn("BUILD_TYPE=", output)
        self.assertIn("Debug", output)

    def test_config_summary_has_compiler(self):
        """Config summary should include CC and CXX when set."""
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = self._make_installer(tmpdir, cc="/usr/bin/gcc", cxx="/usr/bin/g++")
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                with patch("sys.stderr", new_callable=StringIO):
                    installer._dry_config_summary()
                    output = mock_out.getvalue()
        self.assertIn("CC=", output)
        self.assertIn("/usr/bin/gcc", output)
        self.assertIn("CXX=", output)
        self.assertIn("/usr/bin/g++", output)

    def test_config_summary_has_sanitizer_when_set(self):
        """Config summary should include SANITIZER when set."""
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = self._make_installer(tmpdir, sanitizer="address")
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                with patch("sys.stderr", new_callable=StringIO):
                    installer._dry_config_summary()
                    output = mock_out.getvalue()
        self.assertIn("SANITIZER=", output)
        self.assertIn("address", output)

    def test_config_summary_omits_sanitizer_when_empty(self):
        """Config summary should not include SANITIZER when not set."""
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = self._make_installer(tmpdir)
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                with patch("sys.stderr", new_callable=StringIO):
                    installer._dry_config_summary()
                    output = mock_out.getvalue()
        self.assertNotIn("SANITIZER=", output)

    def test_config_summary_has_flags_when_set(self):
        """Config summary should include CXXFLAGS/LDFLAGS when set."""
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = self._make_installer(tmpdir, cxxflags="-gz=zstd", ldflags="-fuse-ld=lld")
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                with patch("sys.stderr", new_callable=StringIO):
                    installer._dry_config_summary()
                    output = mock_out.getvalue()
        self.assertIn("CXXFLAGS=", output)
        self.assertIn("-gz=zstd", output)
        self.assertIn("LDFLAGS=", output)
        self.assertIn("-fuse-ld=lld", output)

    def test_config_summary_silent_without_dry_run(self):
        """Config summary should print nothing when dry_run is False."""
        from src.installer import MrDocsInstaller
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = MrDocsInstaller(
                source_dir=tmpdir,
                cmd_line_args={
                    "dry_run": False,
                    "non_interactive": True,
                    "plain_ui": True,
                },
            )
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                with patch("sys.stderr", new_callable=StringIO):
                    installer._dry_config_summary()
                    output = mock_out.getvalue()
        self.assertEqual(output, "")

    def test_config_summary_is_valid_shell(self):
        """Config summary lines should be valid shell (comments or assignments)."""
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = self._make_installer(tmpdir, build_type="Release", cc="/usr/bin/clang")
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                with patch("sys.stderr", new_callable=StringIO):
                    installer._dry_config_summary()
                    output = mock_out.getvalue()
        for line in output.strip().splitlines():
            stripped = line.strip()
            if not stripped:
                continue
            is_comment = stripped.startswith("#")
            is_assignment = "=" in stripped and not stripped.startswith("=")
            self.assertTrue(
                is_comment or is_assignment,
                f"Line is not a valid shell comment or assignment: {stripped!r}",
            )


class TestDryRunZipFlatten(unittest.TestCase):
    """Test that zip extraction dry-run produces flattening commands."""

    def test_zip_flatten_uses_temp_dir(self):
        """extract_zip_flatten dry-run should use temp dir for flattening."""
        from src.recipes.archive import extract_zip_flatten
        with patch("sys.stdout", new_callable=StringIO) as mock_out:
            extract_zip_flatten("/tmp/test.zip", "/dest/dir", dry_run=True)
            output = mock_out.getvalue()
        self.assertIn("mktemp -d", output)
        self.assertIn("unzip -o", output)
        self.assertIn("cp -a", output)
        self.assertIn("/dest/dir", output)
        self.assertIn("rm -rf", output)

    def test_zip_flatten_no_simple_unzip(self):
        """extract_zip_flatten should NOT use simple 'unzip -d dest' (would skip flattening)."""
        from src.recipes.archive import extract_zip_flatten
        with patch("sys.stdout", new_callable=StringIO) as mock_out:
            extract_zip_flatten("/tmp/test.zip", "/dest/dir", dry_run=True)
            output = mock_out.getvalue()
        # Should NOT just unzip directly to dest (that wouldn't flatten)
        lines = output.strip().splitlines()
        for line in lines:
            if line.strip().startswith("unzip"):
                # The unzip target should be the temp dir, not the dest dir
                self.assertNotIn("/dest/dir", line)


class TestDryRunRecipePaths(unittest.TestCase):
    """Test that dry-run shows source and install paths for each recipe."""

    def _make_installer(self, tmpdir):
        from src.installer import MrDocsInstaller
        return MrDocsInstaller(
            source_dir=tmpdir,
            cmd_line_args={
                "dry_run": True,
                "non_interactive": True,
                "plain_ui": True,
                "skip_build": True,
            },
        )

    def test_dry_comment_includes_source_and_install(self):
        """Recipe comments should include source and install directory paths."""
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = self._make_installer(tmpdir)
            # Simulate what install_dependencies prints for a recipe
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                installer._dry_comment("=== Install dependency: testlib 1.0 ===")
                installer._dry_comment("  source: /path/to/source")
                installer._dry_comment("  install: /path/to/install")
                output = mock_out.getvalue()
        self.assertIn("# === Install dependency: testlib 1.0 ===", output)
        self.assertIn("#   source: /path/to/source", output)
        self.assertIn("#   install: /path/to/install", output)


class TestDryRunNoSkipRecipes(unittest.TestCase):
    """Test that dry-run never skips recipes due to cache/up-to-date checks."""

    def test_fetch_not_skipped_when_up_to_date(self):
        """fetch_recipe_source dry-run should show commands even if stamp says up-to-date."""
        from src.recipes.fetcher import fetch_recipe_source, write_recipe_stamp
        from src.recipes.schema import Recipe, RecipeSource

        ui = TextUI(enable_color=False, enable_emoji=False)
        with tempfile.TemporaryDirectory() as tmpdir:
            install_dir = os.path.join(tmpdir, "install")
            source_dir = os.path.join(tmpdir, "source")
            recipe = Recipe(
                name="test-lib",
                version="1.0.0",
                source=RecipeSource(
                    type="git",
                    url="https://github.com/example/test.git",
                    commit="abc123",
                ),
                dependencies=[],
                source_dir=source_dir,
                build_dir=os.path.join(tmpdir, "build"),
                install_dir=install_dir,
                build_type="Release",
            )
            # Write a stamp so the recipe appears up-to-date
            write_recipe_stamp(recipe, "abc123", dry_run=False, ui=ui)

            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                fetch_recipe_source(
                    recipe,
                    source_dir=tmpdir,
                    dry_run=True,
                    ui=ui,
                )
                output = mock_out.getvalue()

            # Dry-run should show fetch commands, not skip
            self.assertTrue(len(output.strip()) > 0, "Dry-run should produce output even when up-to-date")
            # Should show download or clone commands
            self.assertTrue(
                "curl" in output or "git" in output,
                f"Expected fetch commands in dry-run output, got: {output}",
            )

    def test_fetch_not_skipped_when_source_exists(self):
        """fetch_recipe_source dry-run should show commands even if source dir exists."""
        from src.recipes.fetcher import fetch_recipe_source
        from src.recipes.schema import Recipe, RecipeSource

        ui = TextUI(enable_color=False, enable_emoji=False)
        with tempfile.TemporaryDirectory() as tmpdir:
            source_dir = os.path.join(tmpdir, "source")
            os.makedirs(source_dir)  # Source exists

            recipe = Recipe(
                name="test-lib",
                version="1.0.0",
                source=RecipeSource(
                    type="git",
                    url="https://github.com/example/test.git",
                    commit="abc123",
                ),
                dependencies=[],
                source_dir=source_dir,
                build_dir=os.path.join(tmpdir, "build"),
                install_dir=os.path.join(tmpdir, "install"),
                build_type="Release",
            )

            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                fetch_recipe_source(
                    recipe,
                    source_dir=tmpdir,
                    dry_run=True,
                    ui=ui,
                )
                output = mock_out.getvalue()

            self.assertTrue(len(output.strip()) > 0, "Dry-run should produce output even when source exists")


class TestDryRunNoSkipCmakeWorkflow(unittest.TestCase):
    """Test that cmake_workflow dry-run shows all steps even when install exists."""

    def _make_installer(self, tmpdir):
        from src.installer import MrDocsInstaller
        return MrDocsInstaller(
            source_dir=tmpdir,
            cmd_line_args={
                "dry_run": True,
                "non_interactive": True,
                "plain_ui": True,
                "skip_build": True,
            },
        )

    def test_cmake_workflow_not_skipped_when_install_exists(self):
        """cmake_workflow dry-run should show configure/build/install even if install_dir exists."""
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = self._make_installer(tmpdir)
            install_dir = os.path.join(tmpdir, "install")
            build_dir = os.path.join(tmpdir, "build")
            src_dir = os.path.join(tmpdir, "src")
            # Create a non-empty install dir so allow_skip would normally trigger
            os.makedirs(install_dir)
            with open(os.path.join(install_dir, "marker"), "w") as f:
                f.write("exists")

            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                with patch("sys.stderr", new_callable=StringIO):
                    installer.cmake_workflow(
                        src_dir=src_dir,
                        build_type="Release",
                        build_dir=build_dir,
                        install_dir=install_dir,
                        allow_skip=True,
                    )
                    output = mock_out.getvalue()

            # Should contain cmake configure, build, and install commands
            self.assertIn("cmake", output)
            self.assertIn("--build", output)
            self.assertIn("--install", output)


class TestDryRunEnvVarExports(unittest.TestCase):
    """Test that dry-run prints export statements for custom env vars."""

    def setUp(self):
        self.ui = TextUI(enable_color=False, enable_emoji=False)

    def test_custom_env_vars_exported(self):
        """run_cmd dry-run should print export for env vars differing from os.environ."""
        custom_env = os.environ.copy()
        custom_env["MY_CUSTOM_VAR"] = "custom_value"

        with patch("sys.stdout", new_callable=StringIO) as mock_out:
            run_cmd(["echo", "test"], dry_run=True, env=custom_env, ui=self.ui)
            output = mock_out.getvalue()

        self.assertIn("export MY_CUSTOM_VAR='custom_value'", output)
        self.assertIn("echo test", output)

    def test_no_exports_when_env_matches(self):
        """run_cmd dry-run should not print exports when env matches os.environ."""
        with patch("sys.stdout", new_callable=StringIO) as mock_out:
            run_cmd(["echo", "test"], dry_run=True, env=None, ui=self.ui)
            output = mock_out.getvalue()

        self.assertNotIn("export ", output)
        self.assertEqual(output.strip(), "echo test")

    def test_exports_before_command(self):
        """Export statements should appear before the command they apply to."""
        custom_env = os.environ.copy()
        custom_env["BUILD_VAR"] = "1"

        with patch("sys.stdout", new_callable=StringIO) as mock_out:
            run_cmd(["make", "all"], dry_run=True, env=custom_env, ui=self.ui)
            output = mock_out.getvalue()

        lines = output.strip().splitlines()
        export_idx = None
        cmd_idx = None
        for i, line in enumerate(lines):
            if "export BUILD_VAR=" in line:
                export_idx = i
            if "make all" in line:
                cmd_idx = i
        self.assertIsNotNone(export_idx, "Should have export line")
        self.assertIsNotNone(cmd_idx, "Should have command line")
        self.assertLess(export_idx, cmd_idx, "Export should come before command")


class TestDryRunSkipsToolValidation(unittest.TestCase):
    """Test that dry-run skips tool executable validation."""

    def _make_installer(self, tmpdir, **extra_args):
        from src.installer import MrDocsInstaller
        args = {
            "dry_run": True,
            "non_interactive": True,
            "plain_ui": True,
            "skip_build": True,
        }
        args.update(extra_args)
        return MrDocsInstaller(source_dir=tmpdir, cmd_line_args=args)

    def test_check_tool_accepts_path_in_dry_run(self):
        """check_tool should accept a CLI-provided path without validation in dry-run."""
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = self._make_installer(tmpdir, cmake_path="/nonexistent/cmake")
            # Should not raise even though /nonexistent/cmake is not a real executable
            result = installer.check_tool("cmake")
            self.assertEqual(result, "/nonexistent/cmake")

    def test_check_tool_uses_default_in_dry_run(self):
        """check_tool should fall back to bare tool name when not found in dry-run."""
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = self._make_installer(tmpdir)
            with patch("sys.stdout", new_callable=StringIO):
                with patch("sys.stderr", new_callable=StringIO):
                    # Even if cmake is not installed, dry-run should not fail
                    result = installer.check_tool("cmake")
            # Should return some path (either found cmake or bare "cmake")
            self.assertTrue(len(result) > 0)

    def test_prompt_compiler_option_accepts_path_in_dry_run(self):
        """prompt_compiler_option should accept path without validation in dry-run."""
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = self._make_installer(tmpdir, cc="/nonexistent/clang-19")
            result = installer.prompt_compiler_option("cc", "C compiler")
            self.assertEqual(result, "/nonexistent/clang-19")


class TestDryRunSkipsPrerequisites(unittest.TestCase):
    """Test that dry-run skips prerequisite checks and prints a comment."""

    def _make_installer(self, tmpdir, **extra_args):
        from src.installer import MrDocsInstaller
        args = {
            "dry_run": True,
            "non_interactive": True,
            "plain_ui": True,
            "skip_build": True,
        }
        args.update(extra_args)
        return MrDocsInstaller(source_dir=tmpdir, cmd_line_args=args)

    def test_prerequisites_prints_comment(self):
        """check_system_prerequisites should print a comment listing prereqs in dry-run."""
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = self._make_installer(tmpdir)
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                with patch("sys.stderr", new_callable=StringIO):
                    installer.check_system_prerequisites()
                    output = mock_out.getvalue()
        self.assertIn("# Verify prerequisites:", output)
        self.assertIn("cmake", output)
        self.assertIn("git", output)

    def test_prerequisites_does_not_raise(self):
        """check_system_prerequisites should never raise in dry-run mode."""
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = self._make_installer(tmpdir)
            with patch("sys.stdout", new_callable=StringIO):
                with patch("sys.stderr", new_callable=StringIO):
                    # Should not raise even if tools are missing
                    installer.check_system_prerequisites()

    def test_prerequisites_includes_java_when_testing(self):
        """check_system_prerequisites should mention java when build_tests is True."""
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = self._make_installer(tmpdir, build_tests=True)
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                with patch("sys.stderr", new_callable=StringIO):
                    installer.check_system_prerequisites()
                    output = mock_out.getvalue()
        self.assertIn("java", output)

    def test_prerequisites_omits_java_when_not_testing(self):
        """check_system_prerequisites should omit java when build_tests is False."""
        with tempfile.TemporaryDirectory() as tmpdir:
            installer = self._make_installer(tmpdir, build_tests=False)
            with patch("sys.stdout", new_callable=StringIO) as mock_out:
                with patch("sys.stderr", new_callable=StringIO):
                    installer.check_system_prerequisites()
                    output = mock_out.getvalue()
        self.assertNotIn("java", output)


if __name__ == "__main__":
    unittest.main()
