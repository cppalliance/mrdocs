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

"""Tests for archive.py, fetcher.py, and loader.py remaining paths."""

import io
import json
import os
import shutil
import sys
import tarfile
import tempfile
import unittest
import zipfile
from unittest.mock import patch, MagicMock, call

sys.path.insert(0, str(__file__).rsplit("/", 2)[0])

from src.recipes.schema import RecipeSource, Recipe
from src.recipes.archive import extract_zip_flatten, extract_tar_flatten
from src.recipes.fetcher import (
    build_archive_url,
    recipe_stamp_path,
    is_recipe_up_to_date,
    _build_params,
    _recipe_fields,
    _platform_info,
    write_recipe_stamp,
    download_file,
    fetch_recipe_source,
    apply_recipe_patches,
)
from src.recipes.loader import (
    recipe_placeholders,
    expand_path,
    load_recipe_files,
)
from src.core.ui import TextUI


def _make_recipe(name="test-lib", version="1.0", **kwargs):
    """Helper to create a Recipe with sensible defaults."""
    defaults = dict(
        source=RecipeSource(type="git", url="https://github.com/example/repo.git"),
        dependencies=[],
        source_dir="/src/test-lib",
        build_dir="/build/test-lib",
        install_dir="/install/test-lib",
        build_type="Release",
    )
    defaults.update(kwargs)
    return Recipe(name=name, version=version, **defaults)


# ── archive.py ──────────────────────────────────────────────────────────


class TestExtractZipFlattenDryRun(unittest.TestCase):
    """Test extract_zip_flatten dry-run paths."""

    def test_dry_run_prints_commands(self):
        """Dry-run should print shell commands for zip extraction."""
        buf = io.StringIO()
        with patch("sys.stdout", buf):
            extract_zip_flatten("/tmp/a.zip", "/dest", dry_run=True)
        out = buf.getvalue()
        self.assertIn("unzip", out)
        self.assertIn("/tmp/a.zip", out)
        self.assertIn("/dest", out)
        self.assertIn("mktemp", out)
        self.assertIn("rm -rf", out)


class TestExtractZipFlattenReal(unittest.TestCase):
    """Test extract_zip_flatten with a real zip file."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def _create_zip(self, files):
        """Create a zip with files keyed by archive-internal path."""
        zpath = os.path.join(self.tmpdir, "test.zip")
        with zipfile.ZipFile(zpath, "w") as zf:
            for name, content in files.items():
                zf.writestr(name, content)
        return zpath

    def test_flatten_single_top_level(self):
        """Should strip the common top-level directory."""
        zpath = self._create_zip({
            "repo-main/": "",
            "repo-main/README.md": "hello",
            "repo-main/src/main.py": "print(1)",
        })
        dest = os.path.join(self.tmpdir, "out")
        os.makedirs(dest)
        ui = TextUI()
        extract_zip_flatten(zpath, dest, ui=ui)
        self.assertTrue(os.path.isfile(os.path.join(dest, "README.md")))
        self.assertTrue(os.path.isfile(os.path.join(dest, "src", "main.py")))
        with open(os.path.join(dest, "README.md")) as f:
            self.assertEqual(f.read(), "hello")

    def test_no_top_level_prefix(self):
        """Files without a common prefix should be extracted as-is."""
        zpath = self._create_zip({
            "file_a.txt": "aaa",
            "file_b.txt": "bbb",
        })
        dest = os.path.join(self.tmpdir, "out")
        os.makedirs(dest)
        ui = TextUI()
        extract_zip_flatten(zpath, dest, ui=ui)
        self.assertTrue(os.path.isfile(os.path.join(dest, "file_a.txt")))
        self.assertTrue(os.path.isfile(os.path.join(dest, "file_b.txt")))


class TestExtractTarFlattenDryRun(unittest.TestCase):
    """Test extract_tar_flatten dry-run paths."""

    def test_dry_run_prints_tar_command(self):
        """Dry-run should print tar extraction command."""
        buf = io.StringIO()
        with patch("sys.stdout", buf):
            extract_tar_flatten("/tmp/a.tar.gz", "/dest", dry_run=True)
        out = buf.getvalue()
        self.assertIn("tar xf", out)
        self.assertIn("/tmp/a.tar.gz", out)
        self.assertIn("--strip-components=1", out)


class TestExtractTarFlattenReal(unittest.TestCase):
    """Test extract_tar_flatten with a real tar file."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def test_flatten_tar(self):
        """Should strip the common top-level directory from a tar."""
        tarpath = os.path.join(self.tmpdir, "test.tar.gz")
        with tarfile.open(tarpath, "w:gz") as tf:
            # Add a directory
            d = tarfile.TarInfo(name="repo-main/")
            d.type = tarfile.DIRTYPE
            tf.addfile(d)
            # Add a file
            content = b"hello tar"
            info = tarfile.TarInfo(name="repo-main/README.md")
            info.size = len(content)
            tf.addfile(info, io.BytesIO(content))
        dest = os.path.join(self.tmpdir, "out")
        os.makedirs(dest)
        ui = TextUI()
        extract_tar_flatten(tarpath, dest, ui=ui)
        self.assertTrue(os.path.isfile(os.path.join(dest, "README.md")))
        with open(os.path.join(dest, "README.md")) as f:
            self.assertEqual(f.read(), "hello tar")


# ── fetcher.py ──────────────────────────────────────────────────────────


class TestBuildArchiveUrl(unittest.TestCase):
    """Test build_archive_url."""

    def test_github_commit(self):
        url = build_archive_url("https://github.com/owner/repo.git", "abc123")
        self.assertEqual(url, "https://github.com/owner/repo/archive/abc123.zip")

    def test_github_tag(self):
        url = build_archive_url("https://github.com/owner/repo", "v1.0.0")
        self.assertEqual(url, "https://github.com/owner/repo/archive/v1.0.0.zip")

    def test_github_trailing_slash(self):
        url = build_archive_url("https://github.com/owner/repo/", "main")
        self.assertEqual(url, "https://github.com/owner/repo/archive/main.zip")

    def test_non_github_returns_none(self):
        self.assertIsNone(build_archive_url("https://gitlab.com/o/r", "main"))

    def test_empty_ref_returns_none(self):
        self.assertIsNone(build_archive_url("https://github.com/o/r", ""))

    def test_short_path_returns_none(self):
        self.assertIsNone(build_archive_url("https://github.com/only", "ref"))


class TestRecipeStampPath(unittest.TestCase):
    def test_stamp_path(self):
        r = _make_recipe(install_dir="/install/mylib")
        self.assertEqual(recipe_stamp_path(r), "/install/mylib/.bootstrap-stamp.json")


class TestIsRecipeUpToDate(unittest.TestCase):
    """Test is_recipe_up_to_date."""

    def test_no_stamp_file(self):
        """Missing stamp file means not up to date."""
        r = _make_recipe(install_dir="/nonexistent")
        self.assertNotEqual(is_recipe_up_to_date(r, "abc"), "")

    def test_matching_stamp(self):
        """Stamp with matching version and ref should be up to date."""
        with tempfile.TemporaryDirectory() as td:
            r = _make_recipe(install_dir=td)
            stamp = recipe_stamp_path(r)
            payload = {
                "version": "1.0", "ref": "abc123",
                "recipe": _recipe_fields(r),
                "platform": _platform_info(),
                "build_params": {},
            }
            with open(stamp, "w") as f:
                json.dump(payload, f)
            self.assertEqual(is_recipe_up_to_date(r, "abc123"), "")

    def test_version_mismatch(self):
        """Version change is caught by recipe field comparison."""
        with tempfile.TemporaryDirectory() as td:
            r = _make_recipe(install_dir=td)
            old_recipe = _recipe_fields(r)
            old_recipe["version"] = "2.0"
            stamp = recipe_stamp_path(r)
            with open(stamp, "w") as f:
                json.dump({"version": "2.0", "ref": "abc",
                           "recipe": old_recipe,
                           "build_params": {}}, f)
            self.assertIn("recipe version changed", is_recipe_up_to_date(r, "abc"))

    def test_ref_mismatch(self):
        with tempfile.TemporaryDirectory() as td:
            r = _make_recipe(install_dir=td)
            stamp = recipe_stamp_path(r)
            with open(stamp, "w") as f:
                json.dump({"version": "1.0", "ref": "old"}, f)
            self.assertIn("ref changed", is_recipe_up_to_date(r, "new"))

    def test_corrupt_stamp(self):
        with tempfile.TemporaryDirectory() as td:
            r = _make_recipe(install_dir=td)
            stamp = recipe_stamp_path(r)
            with open(stamp, "w") as f:
                f.write("not json")
            self.assertIn("corrupt", is_recipe_up_to_date(r, "abc"))

    def test_build_params_match(self):
        """Stamp with matching build_params should be up to date."""
        with tempfile.TemporaryDirectory() as td:
            r = _make_recipe(install_dir=td)
            params = _build_params(sanitizer="", cc="/usr/bin/gcc")
            stamp = recipe_stamp_path(r)
            with open(stamp, "w") as f:
                json.dump({"version": "1.0", "ref": "abc",
                           "recipe": _recipe_fields(r),
                           "platform": _platform_info(),
                           "build_params": params}, f)
            self.assertEqual(is_recipe_up_to_date(r, "abc", cc="/usr/bin/gcc"), "")

    def test_build_params_mismatch(self):
        """Stamp with different build_params should report which field changed."""
        with tempfile.TemporaryDirectory() as td:
            r = _make_recipe(install_dir=td)
            params = _build_params(cc="old-gcc")
            stamp = recipe_stamp_path(r)
            with open(stamp, "w") as f:
                json.dump({"version": "1.0", "ref": "abc",
                           "recipe": _recipe_fields(r),
                           "build_params": params}, f)
            self.assertIn("cc changed", is_recipe_up_to_date(r, "abc", cc="new-gcc"))

    def test_recipe_changed(self):
        """Stamp with different recipe fields should report which field changed."""
        with tempfile.TemporaryDirectory() as td:
            r = _make_recipe(install_dir=td)
            old_recipe = _recipe_fields(r)
            old_recipe["build_type"] = "Debug"
            stamp = recipe_stamp_path(r)
            with open(stamp, "w") as f:
                json.dump({"version": "1.0", "ref": "abc",
                           "recipe": old_recipe,
                           "build_params": {}}, f)
            self.assertIn("recipe build_type changed", is_recipe_up_to_date(r, "abc"))

    def test_old_recipe_hash_format(self):
        """Stamp with old recipe_hash should trigger rebuild."""
        with tempfile.TemporaryDirectory() as td:
            r = _make_recipe(install_dir=td)
            stamp = recipe_stamp_path(r)
            with open(stamp, "w") as f:
                json.dump({"version": "1.0", "ref": "abc",
                           "recipe_hash": "old_hash",
                           "build_params": {}}, f)
            self.assertIn("old recipe_hash format", is_recipe_up_to_date(r, "abc"))

    def test_platform_mismatch(self):
        """Stamp from a different platform should trigger rebuild."""
        with tempfile.TemporaryDirectory() as td:
            r = _make_recipe(install_dir=td)
            stamp = recipe_stamp_path(r)
            with open(stamp, "w") as f:
                json.dump({"version": "1.0", "ref": "abc",
                           "recipe": _recipe_fields(r),
                           "platform": {"os": "FakeOS", "arch": "z80", "os_version": "1.0"},
                           "build_params": {}}, f)
            result = is_recipe_up_to_date(r, "abc")
            self.assertIn("platform", result)
            self.assertIn("changed", result)

    def test_old_content_hash_triggers_rebuild(self):
        """Stamp with old content_hash format should trigger rebuild."""
        with tempfile.TemporaryDirectory() as td:
            r = _make_recipe(install_dir=td)
            stamp = recipe_stamp_path(r)
            with open(stamp, "w") as f:
                json.dump({"version": "1.0", "ref": "abc", "content_hash": "old"}, f)
            self.assertIn("old format", is_recipe_up_to_date(r, "abc"))


class TestBuildParams(unittest.TestCase):
    def test_deterministic(self):
        p1 = _build_params(cc="gcc")
        p2 = _build_params(cc="gcc")
        self.assertEqual(p1, p2)

    def test_different_params(self):
        p1 = _build_params(cc="gcc")
        p2 = _build_params(cc="clang")
        self.assertNotEqual(p1, p2)

    def test_empty_when_no_params(self):
        self.assertEqual(_build_params(), {})


class TestWriteRecipeStamp(unittest.TestCase):
    def test_writes_stamp_file(self):
        with tempfile.TemporaryDirectory() as td:
            r = _make_recipe(install_dir=td)
            ui = TextUI()
            write_recipe_stamp(r, "abc123", ui=ui)
            stamp = recipe_stamp_path(r)
            self.assertTrue(os.path.isfile(stamp))
            with open(stamp) as f:
                data = json.load(f)
            self.assertEqual(data["name"], "test-lib")
            self.assertEqual(data["ref"], "abc123")
            self.assertIn("recipe", data)
            self.assertIn("build_params", data)

    def test_dry_run_does_not_write(self):
        """Dry-run should not create stamp file."""
        with tempfile.TemporaryDirectory() as td:
            r = _make_recipe(install_dir=os.path.join(td, "sub"))
            buf = io.StringIO()
            with patch("sys.stdout", buf):
                write_recipe_stamp(r, "ref", dry_run=True, ui=TextUI())
            stamp = recipe_stamp_path(r)
            self.assertFalse(os.path.exists(stamp))


class TestDownloadFile(unittest.TestCase):
    def test_dry_run_prints_curl(self):
        buf = io.StringIO()
        with patch("sys.stdout", buf):
            download_file("https://example.com/f.zip", "/tmp/f.zip", dry_run=True)
        self.assertIn("curl", buf.getvalue())

    @patch("src.recipes.fetcher.urllib.request.urlretrieve")
    def test_downloads_to_dest(self, mock_retrieve):
        with tempfile.TemporaryDirectory() as td:
            dest = os.path.join(td, "file.zip")
            ui = TextUI()
            buf = io.StringIO()
            with patch("sys.stdout", buf):
                download_file("https://example.com/f.zip", dest, ui=ui)
            mock_retrieve.assert_called_once_with("https://example.com/f.zip", dest)

    @patch("src.recipes.fetcher.urllib.request.urlretrieve")
    def test_creates_parent_dir(self, mock_retrieve):
        with tempfile.TemporaryDirectory() as td:
            dest = os.path.join(td, "sub", "deep", "file.zip")
            download_file("https://example.com/f.zip", dest, ui=TextUI())
            self.assertTrue(os.path.isdir(os.path.join(td, "sub", "deep")))


class TestFetchRecipeSource(unittest.TestCase):
    """Test fetch_recipe_source with mocked download and extraction."""

    def test_up_to_date_skips(self):
        """Already up-to-date recipe should skip download."""
        r = _make_recipe(
            source=RecipeSource(type="git", url="https://github.com/o/r.git", commit="abc"),
            source_dir="/tmp/src",
        )
        ui = TextUI()
        with patch("src.recipes.fetcher.is_recipe_up_to_date", return_value=""):
            ref = fetch_recipe_source(r, "/tmp", ui=ui)
        self.assertEqual(ref, "abc")

    def test_source_exists_skips_download(self):
        """Existing source dir without force should skip download."""
        with tempfile.TemporaryDirectory() as td:
            src_dir = os.path.join(td, "src")
            os.makedirs(src_dir)
            r = _make_recipe(
                source=RecipeSource(type="git", url="https://github.com/o/r.git", commit="abc"),
                source_dir=src_dir,
            )
            ui = TextUI()
            with patch("src.recipes.fetcher.is_recipe_up_to_date", return_value="stale"):
                ref = fetch_recipe_source(r, td, ui=ui)
            self.assertEqual(ref, "abc")

    @patch("src.recipes.fetcher.extract_zip_flatten")
    @patch("src.recipes.fetcher.download_file")
    @patch("src.recipes.fetcher.remove_dir")
    @patch("src.recipes.fetcher.ensure_dir")
    @patch("src.recipes.fetcher.is_recipe_up_to_date", return_value="stale")
    def test_archive_fetch_zip(self, mock_uptodate, mock_ensure, mock_remove, mock_dl, mock_extract):
        """GitHub archive should use download + extract_zip_flatten."""
        with tempfile.TemporaryDirectory() as td:
            r = _make_recipe(
                source=RecipeSource(type="git", url="https://github.com/o/r.git", commit="abc123"),
                source_dir=os.path.join(td, "nonexistent"),
            )
            # Mock os.path.exists to return False for source_dir and os.remove
            with patch("src.recipes.fetcher.os.path.exists", return_value=False), \
                 patch("os.remove"):
                ref = fetch_recipe_source(r, td, force=True, ui=TextUI())
            self.assertEqual(ref, "abc123")
            mock_dl.assert_called_once()
            mock_extract.assert_called_once()

    @patch("src.recipes.fetcher.run_cmd")
    @patch("src.recipes.fetcher.ensure_dir")
    @patch("src.recipes.fetcher.is_recipe_up_to_date", return_value="stale")
    def test_git_clone_fallback(self, mock_uptodate, mock_ensure, mock_run):
        """Non-GitHub URL should fall back to git clone."""
        with tempfile.TemporaryDirectory() as td:
            r = _make_recipe(
                source=RecipeSource(type="git", url="https://gitlab.com/o/r.git", branch="main"),
                source_dir=os.path.join(td, "nonexistent"),
            )
            with patch("src.recipes.fetcher.os.path.exists", return_value=False), \
                 patch("src.recipes.fetcher.os.path.isdir", return_value=False):
                ref = fetch_recipe_source(r, td, force=True, ui=TextUI())
            self.assertEqual(ref, "main")
            # Should have called git clone and git pull
            self.assertTrue(mock_run.called)

    def test_dry_run_archive_fetch(self):
        """Dry-run should print commands without actually downloading."""
        r = _make_recipe(
            source=RecipeSource(type="git", url="https://github.com/o/r.git", commit="abc"),
            source_dir="/tmp/src",
        )
        buf = io.StringIO()
        with patch("sys.stdout", buf):
            ref = fetch_recipe_source(r, "/tmp", dry_run=True, ui=TextUI())
        self.assertEqual(ref, "abc")
        out = buf.getvalue()
        self.assertIn("curl", out)

    def test_archive_type_uses_url_directly(self):
        """Source type 'archive' should use the URL directly."""
        r = _make_recipe(
            source=RecipeSource(type="archive", url="https://example.com/pkg.zip"),
            source_dir="/tmp/src",
        )
        buf = io.StringIO()
        with patch("sys.stdout", buf):
            ref = fetch_recipe_source(r, "/tmp", dry_run=True, ui=TextUI())
        self.assertEqual(ref, "HEAD")
        self.assertIn("curl", buf.getvalue())


class TestApplyRecipePatches(unittest.TestCase):
    """Test apply_recipe_patches."""

    def test_no_patch_dir(self):
        """Should do nothing if patch directory doesn't exist."""
        r = _make_recipe(source_dir="/tmp/src")
        # Should not raise
        apply_recipe_patches(r, "/nonexistent/patches", ui=TextUI())

    @patch("src.recipes.fetcher.run_cmd")
    def test_applies_patch_files(self, mock_run):
        """Should run patch command for .patch files."""
        with tempfile.TemporaryDirectory() as td:
            patch_dir = os.path.join(td, "test-lib")
            os.makedirs(patch_dir)
            with open(os.path.join(patch_dir, "01-fix.patch"), "w") as f:
                f.write("patch content")
            r = _make_recipe(source_dir="/tmp/src")
            apply_recipe_patches(r, td, ui=TextUI())
            mock_run.assert_called_once()
            args = mock_run.call_args
            self.assertIn("patch", args[0][0])

    def test_copies_non_patch_file(self):
        """Should copy non-patch files to source dir."""
        with tempfile.TemporaryDirectory() as td:
            patches = os.path.join(td, "test-lib")
            os.makedirs(patches)
            with open(os.path.join(patches, "CMakeLists.txt"), "w") as f:
                f.write("cmake_minimum_required()")
            src = os.path.join(td, "source")
            os.makedirs(src)
            r = _make_recipe(source_dir=src)
            apply_recipe_patches(r, td, ui=TextUI())
            self.assertTrue(os.path.isfile(os.path.join(src, "CMakeLists.txt")))

    def test_copies_directory(self):
        """Should copy directories into source dir."""
        with tempfile.TemporaryDirectory() as td:
            patches = os.path.join(td, "test-lib")
            subdir = os.path.join(patches, "cmake")
            os.makedirs(subdir)
            with open(os.path.join(subdir, "module.cmake"), "w") as f:
                f.write("find_package()")
            src = os.path.join(td, "source")
            os.makedirs(src)
            r = _make_recipe(source_dir=src)
            apply_recipe_patches(r, td, ui=TextUI())
            self.assertTrue(os.path.isfile(os.path.join(src, "cmake", "module.cmake")))

    def test_dry_run_patch_file(self):
        """Dry-run should print cp commands for non-patch files."""
        with tempfile.TemporaryDirectory() as td:
            patches = os.path.join(td, "test-lib")
            os.makedirs(patches)
            with open(os.path.join(patches, "file.txt"), "w") as f:
                f.write("content")
            r = _make_recipe(source_dir="/tmp/src")
            buf = io.StringIO()
            with patch("sys.stdout", buf):
                apply_recipe_patches(r, td, dry_run=True, ui=TextUI())
            self.assertIn("cp", buf.getvalue())

    def test_dry_run_directory(self):
        """Dry-run should print cp -r for directories."""
        with tempfile.TemporaryDirectory() as td:
            patches = os.path.join(td, "test-lib")
            subdir = os.path.join(patches, "cmake")
            os.makedirs(subdir)
            r = _make_recipe(source_dir="/tmp/src")
            buf = io.StringIO()
            with patch("sys.stdout", buf):
                apply_recipe_patches(r, td, dry_run=True, ui=TextUI())
            self.assertIn("cp -r", buf.getvalue())


# ── loader.py ───────────────────────────────────────────────────────────


class TestRecipePlaceholders(unittest.TestCase):
    """Test recipe_placeholders."""

    @patch("src.recipes.loader.is_windows", return_value=False)
    def test_unix_placeholders(self, mock_win):
        r = _make_recipe(build_type="Release")
        ph = recipe_placeholders(r, "release-linux-gcc", cc="/usr/bin/gcc", cxx="/usr/bin/g++")
        self.assertEqual(ph["BOOTSTRAP_BUILD_TYPE"], "Release")
        self.assertEqual(ph["BOOTSTRAP_BUILD_TYPE_LOWER"], "release")
        self.assertEqual(ph["BOOTSTRAP_CC"], "/usr/bin/gcc")
        self.assertEqual(ph["BOOTSTRAP_CXX"], "/usr/bin/g++")
        self.assertEqual(ph["BOOTSTRAP_HOST_PRESET_SUFFIX"], "unix")
        self.assertEqual(ph["BOOTSTRAP_CONFIGURE_PRESET"], "release-linux-gcc")

    @patch("src.recipes.loader.is_windows", return_value=True)
    def test_windows_placeholders(self, mock_win):
        r = _make_recipe(build_type="Debug")
        ph = recipe_placeholders(r, "debug-win-msvc")
        self.assertEqual(ph["BOOTSTRAP_HOST_PRESET_SUFFIX"], "win")
        self.assertEqual(ph["build_type"], "Debug")
        self.assertEqual(ph["build_type_lower"], "debug")


class TestExpandPath(unittest.TestCase):
    """Test expand_path."""

    def test_source_dir_replacement(self):
        result = expand_path("${source_dir}/build", "/home/user/mrdocs", "/tp", "Release")
        self.assertEqual(result, "/home/user/mrdocs/build")

    def test_third_party_replacement(self):
        result = expand_path("${third_party_src_dir}/llvm", "/src", "/tp/src", "Release")
        self.assertEqual(result, "/tp/src/llvm")

    def test_build_type_replacement(self):
        result = expand_path(
            "${source_dir}/build/${build_type_lower}",
            "/src", "/tp", "Release",
        )
        self.assertEqual(result, "/src/build/release")

    def test_empty_template(self):
        self.assertEqual(expand_path("", "/src", "/tp", "Release"), "")

    def test_relative_path_joined(self):
        """Relative result should be joined to source_dir."""
        result = expand_path("relative/path", "/src", "/tp", "Release")
        self.assertEqual(result, os.path.normpath("/src/relative/path"))

    def test_absolute_path_unchanged(self):
        result = expand_path("/absolute/path", "/src", "/tp", "Release")
        self.assertEqual(result, "/absolute/path")


class TestLoadRecipeFiles(unittest.TestCase):
    """Test load_recipe_files with mocked filesystem."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()
        self.recipes_dir = os.path.join(self.tmpdir, "recipes")
        os.makedirs(self.recipes_dir)

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def _write_recipe(self, name, data):
        with open(os.path.join(self.recipes_dir, f"{name}.json"), "w") as f:
            json.dump(data, f)

    def test_loads_single_recipe(self):
        self._write_recipe("llvm", {
            "name": "llvm",
            "version": "18.0",
            "source": {
                "type": "git",
                "url": "https://github.com/llvm/llvm-project.git",
                "commit": "abc123",
            },
            "build_type": "Release",
        })
        recipes = load_recipe_files(
            self.recipes_dir, self.tmpdir, "release-linux-gcc",
            "Release", ui=TextUI(),
        )
        self.assertEqual(len(recipes), 1)
        self.assertEqual(recipes[0].name, "llvm")
        self.assertEqual(recipes[0].version, "18.0")
        # Paths should be set by loader, not the json
        self.assertIn("third-party", recipes[0].source_dir)

    def test_empty_dir(self):
        recipes = load_recipe_files(
            self.recipes_dir, self.tmpdir, "preset", "Release", ui=TextUI(),
        )
        self.assertEqual(recipes, [])

    def test_nonexistent_dir(self):
        recipes = load_recipe_files(
            "/nonexistent", self.tmpdir, "preset", "Release", ui=TextUI(),
        )
        self.assertEqual(recipes, [])

    def test_skips_non_json(self):
        with open(os.path.join(self.recipes_dir, "readme.txt"), "w") as f:
            f.write("not a recipe")
        self._write_recipe("llvm", {
            "name": "llvm",
            "version": "1.0",
            "source": {"type": "git", "url": "https://github.com/o/r.git"},
        })
        recipes = load_recipe_files(
            self.recipes_dir, self.tmpdir, "preset", "Release", ui=TextUI(),
        )
        self.assertEqual(len(recipes), 1)

    def test_skips_corrupt_json(self):
        with open(os.path.join(self.recipes_dir, "bad.json"), "w") as f:
            f.write("not valid json{{{")
        recipes = load_recipe_files(
            self.recipes_dir, self.tmpdir, "preset", "Release", ui=TextUI(),
        )
        self.assertEqual(recipes, [])

    def test_global_install_scope(self):
        self._write_recipe("boost", {
            "name": "boost",
            "version": "1.0",
            "source": {"type": "git", "url": "https://github.com/b/b.git"},
            "install_scope": "global",
        })
        recipes = load_recipe_files(
            self.recipes_dir, self.tmpdir, "release-linux-gcc",
            "Release", ui=TextUI(),
        )
        self.assertEqual(len(recipes), 1)
        # Global scope should NOT include preset in install path
        self.assertNotIn("release-linux-gcc", recipes[0].install_dir)

    def test_per_preset_install_scope(self):
        self._write_recipe("llvm", {
            "name": "llvm",
            "version": "1.0",
            "source": {"type": "git", "url": "https://github.com/l/l.git"},
        })
        recipes = load_recipe_files(
            self.recipes_dir, self.tmpdir, "release-linux-gcc",
            "Release", ui=TextUI(),
        )
        # Per-preset scope should include preset in build/install dirs
        self.assertIn("release-linux-gcc", recipes[0].build_dir)
        self.assertIn("release-linux-gcc", recipes[0].install_dir)

    @patch("src.recipes.loader.is_windows", return_value=False)
    def test_debug_fast_uses_release_preset(self, mock_win):
        """DebugFast builds should use release preset for deps."""
        self._write_recipe("llvm", {
            "name": "llvm",
            "version": "1.0",
            "source": {"type": "git", "url": "https://github.com/l/l.git"},
        })
        recipes = load_recipe_files(
            self.recipes_dir, self.tmpdir, "debug-fast-linux-gcc",
            "DebugFast", ui=TextUI(),
        )
        self.assertIn("release", recipes[0].build_dir.lower())

    def test_placeholder_substitution(self):
        self._write_recipe("llvm", {
            "name": "llvm",
            "version": "1.0",
            "source": {
                "type": "git",
                "url": "https://github.com/l/l.git",
                "tag": "v${build_type_lower}",
            },
        })
        recipes = load_recipe_files(
            self.recipes_dir, self.tmpdir, "release-linux-gcc",
            "Release", ui=TextUI(),
        )
        self.assertEqual(recipes[0].source.tag, "vrelease")

    def test_name_defaults_to_filename(self):
        """Recipe without name field should use filename."""
        self._write_recipe("mylib", {
            "version": "1.0",
            "source": {"type": "git", "url": "https://example.com/r.git"},
        })
        recipes = load_recipe_files(
            self.recipes_dir, self.tmpdir, "preset", "Release", ui=TextUI(),
        )
        self.assertEqual(recipes[0].name, "mylib")


if __name__ == "__main__":
    unittest.main()
