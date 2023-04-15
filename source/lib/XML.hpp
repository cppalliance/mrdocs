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

#ifndef MRDOX_SOURCE_XML_HPP
#define MRDOX_SOURCE_XML_HPP

#include <mrdox/ForwardDecls.hpp>
#include <mrdox/Generator.hpp>
#include <mrdox/RecursiveWriter.hpp>

namespace clang {
namespace mrdox {

//------------------------------------------------

struct XMLGenerator : Generator
{
    class Writer;

    llvm::StringRef
    name() const noexcept override
    {
        return "XML";
    }

    llvm::StringRef
    extension() const noexcept override
    {
        return "xml";
    }

    bool
    buildOne(
        llvm::StringRef fileName,
        Corpus& corpus,
        Config const& config,
        Reporter& R) const override;

    bool
    buildString(
        std::string& dest,
        Corpus& corpus,
        Config const& config,
        Reporter& R) const override;
};

//------------------------------------------------

/** A writer which outputs XML.
*/
class XMLGenerator::Writer
    : public RecursiveWriter
{
public:
    Writer(
        llvm::raw_ostream& os,
        Corpus const& corpus,
        Config const& config,
        Reporter& R) noexcept;

private:
    struct Attr;
    using Attrs = std::initializer_list<Attr>;

    void writeAllSymbols(std::vector<AllSymbol> const& list) override;

    void beginFile() override;
    void endFile() override;

    void beginNamespace(NamespaceInfo const& I) override;
    void writeNamespace(NamespaceInfo const& I) override;
    void endNamespace(NamespaceInfo const& I) override;

    void beginRecord(RecordInfo const& I) override;
    void writeRecord(RecordInfo const& I) override;
    void endRecord(RecordInfo const& I) override;

    void beginFunction(FunctionInfo const& I) override;
    void writeFunction(FunctionInfo const& I) override;
    void endFunction(FunctionInfo const& I) override;

    void writeEnum(EnumInfo const& I) override;
    void writeTypedef(TypedefInfo const& I) override;

    void writeInfo(Info const& I);
    void writeSymbol(SymbolInfo const& I);
    void writeLocation(Location const& loc, bool def = false);
    void writeBaseRecord(BaseRecordInfo const& I);
    void writeParam(FieldTypeInfo const& I);
    void writeTemplateParam(TemplateParamInfo const& I);
    void writeMemberType(MemberTypeInfo const& I);

    void openTag(llvm::StringRef);
    void openTag(llvm::StringRef, Attrs);
    void closeTag(llvm::StringRef);
    void writeTag(llvm::StringRef);
    void writeTag(llvm::StringRef, Attrs);
    void writeTagLine(llvm::StringRef tag, llvm::StringRef value);
    void writeTagLine(llvm::StringRef tag, llvm::StringRef value, Attrs);
    void writeAttrs(Attrs attrs);

    static std::string toString(SymbolID const& id);
    static llvm::StringRef toString(InfoType) noexcept;

    //--------------------------------------------

    struct escape;

    struct Attr
    {
        llvm::StringRef name;
        std::string value;
        bool pred;

        Attr(
            llvm::StringRef name_,
            llvm::StringRef value_,
            bool pred_ = true) noexcept
            : name(name_)
            , value(value_)
            , pred(pred_)
        {
        }

        Attr(AccessSpecifier access) noexcept
            : name("access")
            , value(clang::getAccessSpelling(access))
            , pred(access != AccessSpecifier::AS_none)
        {
        }

        Attr(SymbolID USR)
            : name("id")
            , value(toString(USR))
            , pred(USR != EmptySID)
        {
        }
    };
};

//------------------------------------------------

} // mrdox
} // clang

#endif
