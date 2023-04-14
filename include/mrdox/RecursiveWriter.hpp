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

#ifndef MRDOX_RECURSIVE_WRITER_HPP
#define MRDOX_RECURSIVE_WRITER_HPP

#include <mrdox/Info.hpp>
#include <mrdox/Reporter.hpp>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/raw_ostream.h>
#include <string>

namespace clang {
namespace mrdox {

//------------------------------------------------

class Corpus;
class Config;
struct NamespaceInfo;
struct RecordInfo;
struct FunctionInfo;
struct EnumInfo;
struct TypedefInfo;
struct Scope;

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
    llvm::raw_fd_ostream* fd_os_ = nullptr;
    llvm::raw_string_ostream* str_os_ = nullptr;
    std::string indentString_;

protected:
    Corpus const& corpus_;
    Config const& config_;
    Reporter& R_;

    llvm::raw_ostream& os() noexcept;
    llvm::raw_ostream& indent();
    void adjustNesting(int levels);

public:
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

        AllSymbol(Info const& I);
    };

    /** Destructor.
    */
    ~RecursiveWriter() = default;

    /** Constructor.
    */
    RecursiveWriter(
        Corpus const& corpus,
        Config const& config,
        Reporter& R) noexcept;

    /** Write the contents of the corpus to the output stream.
    */
    /** @{ */
    void write(llvm::raw_fd_ostream& os);
    void write(llvm::raw_string_ostream& os);
    /** @} */

    /** Called to open and close the document.

        The default implementation does nothing.
    */
    /** @{ */
    virtual void beginDoc();
    virtual void endDoc();
    /** @} */

    /** Called to write all symbols.

        Each element contains the fully qualified
        name and the type of symbol. The list is
        canonicalized by a visual sort on symbol.
    */
    virtual void writeAllSymbols(std::vector<AllSymbol> const& list);

    /** Called to open or close a scope.

        The default implementation does nothing.
    */
    /** @{ */
    virtual void beginNamespace(NamespaceInfo const& I);
    virtual void writeNamespace(NamespaceInfo const& I);
    virtual void endNamespace(NamespaceInfo const& I);

    virtual void beginRecord(RecordInfo const& I);
    virtual void writeRecord(RecordInfo const& I);
    virtual void endRecord(RecordInfo const& I);

    virtual void beginFunction(FunctionInfo const& I);
    virtual void writeFunction(FunctionInfo const& I);
    virtual void endFunction(FunctionInfo const& I);

    virtual void writeEnum(EnumInfo const& I);

    virtual void writeTypedef(TypedefInfo const& I);
    /** @} */

private:
    void visit(NamespaceInfo const&);
    void visit(RecordInfo const&);
    void visit(Scope const&);
    void visit(FunctionInfo const&);

    std::vector<AllSymbol> makeAllSymbols();
};

} // mrdox
} // clang

#endif
