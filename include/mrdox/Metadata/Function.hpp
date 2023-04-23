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

#ifndef MRDOX_FUNCTION_HPP
#define MRDOX_FUNCTION_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/FieldType.hpp>
#include <mrdox/Metadata/Function.hpp>
#include <mrdox/Metadata/Symbol.hpp>
#include <mrdox/Metadata/Template.hpp>
#include <mrdox/Metadata/Symbols.hpp>
#include <clang/Basic/Specifiers.h>
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/SmallVector.h>
#include <vector>

namespace clang {
namespace mrdox {

// TODO: Expand to allow for documenting templating and default args.
// Info for functions.
struct FunctionInfo : SymbolInfo
{
    /** Bit constants used with function specifiers.
    */
    enum : std::uint32_t
    {
        // 11 bits
        constBit         = 0x00000001,
        constevalBit     = 0x00000002,
        constexprBit     = 0x00000004,
        inlineBit        = 0x00000008,
        noexceptBit      = 0x00000010,
        noreturnBit      = 0x00000020,
        overrideBit      = 0x00000040,
        pureBit          = 0x00000080,
        specialBit       = 0x00000100, // dtor, move/copy construct or assign
        trailReturnBit   = 0x00000200,
        variadicBit      = 0x00000400, // has a C-style "..." variadic 
        virtualBit       = 0x00000800,
        volatileBit      = 0x00001000,

        refQualifierMask = 0x18000000, // 2 bits
        storageClassMask = 0xE0000000  // top 3 bits
    };

    /** Specifiers for the function.

        This is its own object to help the
        serializer out with converting to and
        from bitcode, and to help with merging.
    */
    class Specs
    {
    public:
        using value_type = std::uint32_t;

        Specs() = default;

        explicit
        Specs(value_type bits) noexcept
            : bits_(bits)
        {
        }

        value_type bits() const noexcept
        {
            return bits_;
        }

        bool isSet(value_type bit) const noexcept
        {
            return bits_ & bit;
        }

        RefQualifierKind refQualifier() const noexcept
        {
            return static_cast<RefQualifierKind>((
                bits_ & refQualifierMask) >> 11);
        }

        StorageClass storageClass() const noexcept
        {
            return static_cast<StorageClass>((
                bits_ & storageClassMask) >> 13);
        }

        void set(value_type bit, bool value = true) noexcept
        {
            if(value)
                bits_ |= bit;
            else
                bits_ &= ~bit;
        }

        void setRefQualifier(RefQualifierKind k) noexcept
        {
            bits_ |= (static_cast<value_type>(k) << 11);
        }

        void setStorageClass(StorageClass sc) noexcept
        {
            bits_ |= (static_cast<value_type>(sc) << 13);
        }

        void merge(Specs&& other) noexcept;

    private:
        value_type bits_ = 0;
    };

    bool IsMethod = false; // Indicates whether this function is a class method.
    Reference Parent;      // Reference to the parent class decl for this method.
    TypeInfo ReturnType;   // Info about the return type of this function.
    llvm::SmallVector<FieldTypeInfo, 4> Params; // List of parameters.

    // Access level for this method (public, private, protected, none).
    // AS_public is set as default because the bitcode writer requires the enum
    // with value 0 to be used as the default.
    // (AS_public = 0, AS_protected = 1, AS_private = 2, AS_none = 3)
    AccessSpecifier Access = AccessSpecifier::AS_public;

    // Full qualified name of this function, including namespaces and template
    // specializations.
    SmallString<16> FullName;

    // When present, this function is a template or specialization.
    llvm::Optional<TemplateInfo> Template;

    Specs specs;

    //--------------------------------------------

    static constexpr InfoType type_id = InfoType::IT_function;

    explicit
    FunctionInfo(
        SymbolID id_ = SymbolID())
        : SymbolInfo(InfoType::IT_function, id_)
    {
    }

    void merge(FunctionInfo&& I);
};

//------------------------------------------------

} // mrdox
} // clang

#endif