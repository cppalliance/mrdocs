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

#ifndef MRDOX_API_AST_DECODERECORD_HPP
#define MRDOX_API_AST_DECODERECORD_HPP

#include "BitcodeReader.hpp"

namespace clang {
namespace mrdox {

// bool
inline
llvm::Error
decodeRecord(
    Record const& R,
    bool& Field,
    llvm::StringRef Blob)
{
    Field = R[0] != 0;
    return llvm::Error::success();
}

// integral types
template<class IntTy>
requires std::is_integral_v<IntTy>
llvm::Error
decodeRecord(
    Record const& R,
    IntTy& v,
    llvm::StringRef Blob)
{
    v = 0;
    if (R[0] > (std::numeric_limits<IntTy>::max)())
        return makeError("integer overflow");
    v = static_cast<IntTy>(R[0]);
    return llvm::Error::success();
}

// enumerations
template<class Enum>
requires std::is_enum_v<Enum>
llvm::Error
decodeRecord(
    Record const& R,
    Enum& value,
    llvm::StringRef blob)
{
    std::underlying_type_t<Enum> temp;
    if(auto err = decodeRecord(R, temp, blob))
        return err;
    value = static_cast<Enum>(temp);
    return llvm::Error::success();
}

// container of char
template<class Field>
requires std::is_same_v<
    typename Field::value_type, char>
llvm::Error
decodeRecord(
    const Record& R,
    Field& f,
    llvm::StringRef blob)
        requires requires
        {
            f.assign(blob.begin(), blob.end());
        }
{
    f.assign(blob.begin(), blob.end());
    return llvm::Error::success();
}

// range<SymbolID>
inline
llvm::Error
decodeRecord(
    Record const& R,
    llvm::SmallVectorImpl<SymbolID>& f,
    llvm::StringRef blob)
{
    auto src = R.begin();
    auto n = *src++;
    f.resize(n);
    auto* dest = &f[0];
    while(n--)
    {
        *dest++ = SymbolID(src);
        src += BitCodeConstants::USRHashSize;
    }
    return llvm::Error::success();
}

// vector<RefWithAccess>
inline
llvm::Error
decodeRecord(
    Record const& R,
    std::vector<RefWithAccess>& f,
    llvm::StringRef blob)
{
    constexpr auto RefWithAccessSize =
        BitCodeConstants::USRHashSize + 1;
    if(R.empty())
        return makeError("empty");
    auto n = R.size() / RefWithAccessSize;
    if(R.size() != n * RefWithAccessSize)
        return makeError("short Record");
    auto src = R.begin();
    f.resize(n);
    auto* dest = &f[0];
    while(n--)
    {
        if(*src > 2) // Access::Private
            return makeError("invalid Access");
        dest->access = static_cast<Access>(*src++);
        dest->id = SymbolID(src);
        ++dest;
        src += BitCodeConstants::USRHashSize;
    }
    return llvm::Error::success();
}

inline
llvm::Error
decodeRecord(
    const Record& R,
    SymbolID& Field,
    llvm::StringRef Blob)
{
    if (R[0] != BitCodeConstants::USRHashSize)
        return makeError("incorrect USR digest size");

    Field = SymbolID(&R[1]);
    return llvm::Error::success();
}

inline
llvm::Error
decodeRecord(
    Record const& R,
    AccessSpecifier& Field,
    llvm::StringRef Blob)
{
    switch (R[0])
    {
    case AS_public:
    case AS_private:
    case AS_protected:
    case AS_none:
        Field = (AccessSpecifier)R[0];
        return llvm::Error::success();
    default:
        Field = AS_none;
        return makeError("invalid value for AccessSpecifier");
    }
}

inline
llvm::Error
decodeRecord(
    Record const& R,
    TagTypeKind& Field,
    llvm::StringRef Blob)
{
    switch (R[0])
    {
    case TTK_Struct:
    case TTK_Interface:
    case TTK_Union:
    case TTK_Class:
    case TTK_Enum:
        Field = (TagTypeKind)R[0];
        return llvm::Error::success();
    default:
        Field = TTK_Struct;
        return makeError("invalid value for TagTypeKind");
    }
}

inline
llvm::Error
decodeRecord(
    Record const& R,
    llvm::Optional<Location>& Field,
    llvm::StringRef Blob)
{
    if (R[0] > INT_MAX)
        return makeError("integer too large to parse");
    Field.emplace((int)R[0], Blob, (bool)R[1]);
    return llvm::Error::success();
}

inline
llvm::Error
decodeRecord(
    Record const& R,
    InfoType& Field,
    llvm::StringRef Blob)
{
    switch (auto IT = static_cast<InfoType>(R[0]))
    {
    case InfoType::IT_namespace:
    case InfoType::IT_record:
    case InfoType::IT_function:
    case InfoType::IT_enum:
    case InfoType::IT_typedef:
    case InfoType::IT_variable:
        Field = IT;
        return llvm::Error::success();
    default:
        Field = InfoType::IT_default;
        return makeError("invalid value for InfoType");
    }
}

inline
llvm::Error
decodeRecord(
    Record const& R,
    FieldId& Field,
    llvm::StringRef Blob)
{
    switch (auto F = static_cast<FieldId>(R[0]))
    {
    case FieldId::F_namespace:
    case FieldId::F_vparent:
    case FieldId::F_type:
    case FieldId::F_child_namespace:
    case FieldId::F_child_record:
    case FieldId::F_child_function:
    case FieldId::F_child_typedef:
    case FieldId::F_child_enum:
    case FieldId::F_child_variable:
    case FieldId::F_default:
        Field = F;
        return llvm::Error::success();
    }
    Field = FieldId::F_default;
    return makeError("invalid value for FieldId");
}

inline
llvm::Error
decodeRecord(
    const Record& R,
    llvm::SmallVectorImpl<Location>& Field,
    llvm::StringRef Blob)
{
    if (R[0] > INT_MAX)
        return makeError("integer too large to parse");
    Field.emplace_back((int)R[0], Blob, (bool)R[1]);
    return llvm::Error::success();
}

inline
llvm::Error
decodeRecord(
    Record const& R,
    std::initializer_list<BitFieldFullValue*> values,
    llvm::StringRef Blob)
{
    auto n = R[0];
    if(n != values.size())
        return makeError("wrong size(", n, ") for Bitfields[", values.size(), "]");

    auto itr = values.begin();
    for(std::size_t i = 0; i < values.size(); ++i)
    {

        auto const v = R[i + 1];
        if(v > (std::numeric_limits<std::uint32_t>::max)())
            return makeError(v, " is out of range for Bits");
        **itr++ = v;
    }
    return llvm::Error::success();
}

} // mrdox
} // clang

#endif
