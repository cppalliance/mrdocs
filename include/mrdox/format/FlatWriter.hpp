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

#ifndef MRDOX_FLATWRITER_HPP
#define MRDOX_FLATWRITER_HPP

#include <mrdox/MetadataFwd.hpp>
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

/** An abstract writer for flat output formats.
*/
class FlatWriter
{
protected:
    // The stream being written to
    llvm::raw_ostream& os_;

    // Path to file being written, or empty
    llvm::StringRef filePath_;

    Corpus const& corpus_;
    Config const& config_;
    Reporter& R_;

    /** Constructor.
    */
    FlatWriter(
        llvm::raw_ostream& os,
        llvm::StringRef filePath,
        Corpus const& corpus,
        Config const& config,
        Reporter& R) noexcept;

public:
    /** Destructor.
    */
    virtual ~FlatWriter() = default;

    void visitAllSymbols();
    void visit(SymbolID const& id);

    virtual void beginFile();
    virtual void endFile();

protected:
    virtual void writeNamespace(NamespaceInfo const& I);
    virtual void writeRecord(RecordInfo const& I);
    virtual void writeFunction(FunctionInfo const& I);
    virtual void writeEnum(EnumInfo const& I);
    virtual void writeTypedef(TypedefInfo const& I);

private:
    void visit(NamespaceInfo const&);
    void visit(RecordInfo const&);
    void visit(FunctionInfo const&);
    void visit(Scope const&);
};

} // mrdox
} // clang

#endif
