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

#ifndef MRDOX_TOOL_AST_DECODERECORD_HPP
#define MRDOX_TOOL_AST_DECODERECORD_HPP

#include "BitcodeReader.hpp"

namespace clang {
namespace mrdox {

// bool
inline
Error
decodeRecord(
    Record const& R,
    bool& Field,
    llvm::StringRef Blob)
{
    Field = R[0] != 0;
    return Error::success();
}

// integral types
template<class IntTy>
requires std::is_integral_v<IntTy>
Error
decodeRecord(
    Record const& R,
    IntTy& v,
    llvm::StringRef Blob)
{
    v = 0;
    if (R[0] > (std::numeric_limits<IntTy>::max)())
        return Error("integer overflow");
    v = static_cast<IntTy>(R[0]);
    return Error::success();
}

// enumerations
template<class Enum>
requires std::is_enum_v<Enum>
Error
decodeRecord(
    Record const& R,
    Enum& value,
    llvm::StringRef blob)
{
    std::underlying_type_t<Enum> temp;
    if(auto err = decodeRecord(R, temp, blob))
        return err;
    value = static_cast<Enum>(temp);
    return Error::success();
}

// container of char
template<class Field>
requires std::is_same_v<
    typename Field::value_type, char>
Error
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
    return Error::success();
}

// range<SymbolID>
inline
Error
decodeRecord(
    Record const& R,
    std::vector<SymbolID>& f,
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
    return Error::success();
}

inline
Error
decodeRecord(
    const Record& R,
    SymbolID& Field,
    llvm::StringRef Blob)
{
    if (R[0] != BitCodeConstants::USRHashSize)
        return Error("USR digest size={}", R[0]);

    Field = SymbolID(&R[1]);
    return Error::success();
}

inline
Error
decodeRecord(
    Record const& R,
    OptionalLocation& Field,
    llvm::StringRef Blob)
{
    if (R[0] > INT_MAX)
        return Error("integer value {} too large", R[0]);
    Field.emplace((int)R[0], Blob, (bool)R[1]);
    return Error::success();
}

inline
Error
decodeRecord(
    Record const& R,
    InfoKind& Kind,
    llvm::StringRef Blob)
{
    Kind = static_cast<InfoKind>(R[0]);
    switch(Kind)
    {
    case InfoKind::Namespace:
    case InfoKind::Record:
    case InfoKind::Function:
    case InfoKind::Enum:
    case InfoKind::Typedef:
    case InfoKind::Variable:
    case InfoKind::Field:
    case InfoKind::Specialization:
        return Error::success();
    default:
        return Error("InfoKind is invalid");
    }
}

inline
Error
decodeRecord(
    const Record& R,
    std::vector<Location>& Field,
    llvm::StringRef Blob)
{
    if (R[0] > INT_MAX)
        return Error("integer {} is too large", R[0]);
    Field.emplace_back((int)R[0], Blob, (bool)R[1]);
    return Error::success();
}

inline
Error
decodeRecord(
    Record const& R,
    std::initializer_list<BitFieldFullValue*> values,
    llvm::StringRef Blob)
{
    auto n = R[0];
    if(n != values.size())
        return Error("wrong size={} for Bitfields[{}]", n, values.size());

    auto itr = values.begin();
    for(std::size_t i = 0; i < values.size(); ++i)
    {

        auto const v = R[i + 1];
        if(v > (std::numeric_limits<std::uint32_t>::max)())
            return Error("{} is out of range for Bits", v);
        **itr++ = v;
    }
    return Error::success();
}

} // mrdox
} // clang

#endif
