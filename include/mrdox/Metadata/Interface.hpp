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

#ifndef MRDOX_API_METADATA_INTERFACE_HPP
#define MRDOX_API_METADATA_INTERFACE_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Metadata/Record.hpp>
#include <mrdox/Support/SharedPtr.hpp>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace clang {
namespace mrdox {

/** The aggregated interface for a given struct, class, or union.
*/
class Interface
{
public:
    /** A group of children that have the same access specifier.
    */
    struct Tranche
    {
        std::span<RecordInfo const*>    Records;
        std::span<FunctionInfo const*>  Functions;
        std::span<EnumInfo const*>      Enums;
        std::span<TypedefInfo const*>   Types;
        std::span<FieldInfo const*>     Data;
        std::span<FunctionInfo const*>  StaticFunctions;
        std::span<VariableInfo const*>  StaticData;
    };

    Corpus const& corpus;

    /** The aggregated public interfaces.
    */
    Tranche Public;

    /** The aggregated protected interfaces.
    */
    Tranche Protected;

    /** The aggregated private interfaces.
    */
    Tranche Private;

    MRDOX_DECL
    friend
    SharedPtr<Interface>
    makeInterface(
        RecordInfo const& Derived,
        Corpus const& corpus);

private:
    class Build;

    explicit Interface(Corpus const&) noexcept;

    std::vector<RecordInfo const*>    records_;
    std::vector<FunctionInfo const*>  functions_;
    std::vector<EnumInfo const*>      enums_;
    std::vector<TypedefInfo const*>   types_;
    std::vector<FieldInfo const*>     data_;
    std::vector<FunctionInfo const*>  staticfuncs_;
    std::vector<VariableInfo const*>  staticdata_;
};

//------------------------------------------------

/** Return the composite interface for a record.

    @return The interface.

    @param I The interface to store the results in.

    @param Derived The most derived record to start from.

    @param corpus The complete metadata.
*/
MRDOX_DECL
SharedPtr<Interface>
makeInterface(
    RecordInfo const& Derived,
    Corpus const& corpus);

} // mrdox
} // clang

#endif
