//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_RECURSIVEWRITER_HPP
#define MRDOX_RECURSIVEWRITER_HPP

#include <mrdox/MetadataFwd.hpp>
#include <mrdox/meta/Symbols.hpp>
#include <mrdox/Reporter.hpp>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/raw_ostream.h>
#include <string>

namespace clang {
namespace mrdox {

class Corpus;
class Config;

//------------------------------------------------

/** An abstract writer for recursive output formats.

    The implementation visits the global namespace
    and then each child namespace recursively. The
    scope for each namespace is also iterated and
    emitted. Subclasses should override the relevant
    visitation functions (the default implementations
    do nothing).

    This base class is suitable for writing a single
    file using a recursive syntax such as that found
    in XML, HTML, or JSON.
*/
class RecursiveWriter
{
    std::string indentString_;

protected:
    llvm::raw_ostream& os_;
    Corpus const& corpus_;
    Reporter& R_;

    /** Constructor.
    */
    RecursiveWriter(
        llvm::raw_ostream& os,
        Corpus const& corpus,
        Reporter& R) noexcept;

    /** Destructor.
    */
    virtual ~RecursiveWriter() = default;

    /** Describes an item in the list of all symbols.
    */
    struct AllSymbol
    {
        /** The fully qualified name of this symbol.
        */
        std::string fqName;

        /** A string representing the symbol type.
        */
        llvm::StringRef symbolType;

        /** The ID of this symbol.
        */
        SymbolID id;

        /** Constructor.
        */
        AllSymbol(Info const& I);
    };

    virtual void visitNamespace(NamespaceInfo const&) = 0;
    virtual void visitRecord(RecordInfo const&) = 0;
    virtual void visitFunction(FunctionInfo const&) = 0;
    virtual void visitTypedef(TypedefInfo const& I) = 0;
    virtual void visitEnum(EnumInfo const& I) = 0;

    void visitScope(Scope const&);
    std::vector<AllSymbol> makeAllSymbols();

    llvm::raw_ostream& indent();
    void adjustNesting(int levels);
};

} // mrdox
} // clang

#endif
