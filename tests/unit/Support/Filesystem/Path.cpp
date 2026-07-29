//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Support/Filesystem/Temp.hpp>
#include <test_suite/test_suite.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>


namespace mrdocs {

struct Path_test
{
    void
    testPaths()
    {
        using namespace files;
        
    /*
        BOOST_TEST(isAbsolute("C:\\"));
        BOOST_TEST(isAbsolute("/"));
        BOOST_TEST(isAbsolute("/etc"));
        BOOST_TEST(isAbsolute("\\"));
    */
    }

    void
    testIsSubpathOf()
    {
        using namespace files;

        // empty
        {
            BOOST_TEST(isSubpathOf("", ""));
        }

        // identical
        {
            BOOST_TEST(isSubpathOf("/", "/"));
            BOOST_TEST(isSubpathOf("/abc", "/abc"));
            BOOST_TEST(isSubpathOf("/abc/def", "/abc/def"));
        }

        // equivalent
        {
            BOOST_TEST(isSubpathOf("/", "\\"));
            BOOST_TEST(isSubpathOf("/abc", "\\abc"));
            BOOST_TEST(isSubpathOf("\\abc", "/abc"));
            BOOST_TEST(isSubpathOf("/abc/def", "\\abc\\def"));
            BOOST_TEST(isSubpathOf("\\abc\\def", "/abc/def"));
        }

        // subdirectory
        {
            BOOST_TEST(isSubpathOf("/abc/def", "/abc"));
            BOOST_TEST(isSubpathOf("\\abc\\def", "/abc"));
            BOOST_TEST_NOT(isSubpathOf("/abcdef", "/abc"));
            BOOST_TEST_NOT(isSubpathOf("\\abcdef", "/abc"));
        }
    }

    void
    testRealPathAndResolvedSubpath()
    {
        using namespace files;
        namespace fs = llvm::sys::fs;
        namespace path = llvm::sys::path;

        // Build a temp tree with a real file and a directory symlink:
        //   <tmp>/real/include/demo/widget.hpp   (a real file)
        //   <tmp>/link  ->  <tmp>/real           (a directory symlink)
        llvm::SmallString<128> tmp;
        if (fs::createUniqueDirectory("mrdocs-canon-test", tmp))
        {
            return; // cannot create temp dir; skip
        }
        llvm::SmallString<256> realIncludeDemo(tmp);
        path::append(realIncludeDemo, "real", "include", "demo");
        if (fs::create_directories(realIncludeDemo))
        {
            fs::remove_directories(tmp);
            return;
        }
        llvm::SmallString<256> header(realIncludeDemo);
        path::append(header, "widget.hpp");
        {
            std::error_code ec;
            llvm::raw_fd_ostream os(header.str(), ec);
            // real_path needs the file to exist; an empty file is enough.
        }
        llvm::SmallString<256> realDir(tmp);
        path::append(realDir, "real");
        llvm::SmallString<256> link(tmp);
        path::append(link, "link");
        if (fs::create_link(realDir.str(), link.str()))
        {
            // Symlinks may be unsupported (e.g. Windows without privilege):
            // skip the symlink-dependent assertions.
            fs::remove_directories(tmp);
            return;
        }

        // The temp dir itself may sit under a symlink (e.g. /var on macOS),
        // so compute the resolved base for the expectations.
        llvm::SmallString<256> tmpReal;
        if (fs::real_path(tmp, tmpReal))
        {
            fs::remove_directories(tmp);
            return;
        }

        // A file reached through the symlinked directory resolves to its
        // real location.
        llvm::SmallString<256> viaLink(tmp);
        path::append(viaLink, "link", "include", "demo", "widget.hpp");
        llvm::SmallString<256> realHeader(tmpReal);
        path::append(realHeader, "real", "include", "demo", "widget.hpp");
        BOOST_TEST(makeRealPath(viaLink.str())
            == makePosixStyle(realHeader.str()));

        // A path with no symlinks is unchanged apart from POSIX style: this
        // is why filters over a tree with no symlinks behave as before.
        BOOST_TEST(makeRealPath(realHeader.str())
            == makePosixStyle(realHeader.str()));

        // A path that does not exist is returned as written (fallback), so
        // virtual/in-memory files never break the comparison.
        llvm::SmallString<256> missing(tmpReal);
        path::append(missing, "real", "include", "demo", "missing.hpp");
        BOOST_TEST(makeRealPath(missing.str())
            == makePosixStyle(missing.str()));

        // Generous inclusion: the header spelled through the symlink counts
        // as being under the real include directory, even though the literal
        // prefixes differ.
        llvm::SmallString<256> realInclude(tmpReal);
        path::append(realInclude, "real", "include");
        BOOST_TEST(isResolvedSubpathOf(viaLink.str(), realInclude.str()));
        BOOST_TEST_NOT(isSubpathOf(viaLink.str(), realInclude.str()));

        // A file genuinely outside the directory is not included.
        llvm::SmallString<256> outside(tmpReal);
        path::append(outside, "real", "other.hpp");
        BOOST_TEST_NOT(isResolvedSubpathOf(outside.str(), realInclude.str()));

        fs::remove_directories(tmp);
    }

    void run()
    {
        testPaths();
        testIsSubpathOf();
        testRealPathAndResolvedSubpath();
    }
};

TEST_SUITE(
    Path_test,
    "clang.mrdocs.Path");

} // mrdocs


