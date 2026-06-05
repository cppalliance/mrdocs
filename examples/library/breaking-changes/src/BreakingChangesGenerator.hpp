//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//

#ifndef MRDOCS_EXAMPLE_BREAKING_CHANGES_GENERATOR_HPP
#define MRDOCS_EXAMPLE_BREAKING_CHANGES_GENERATOR_HPP

#include <mrdocs/Corpus.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/Error.hpp>

#include <ostream>
#include <string_view>

namespace mrdocs::example {

// tag::generator-class[]
// A Generator subclass that emits a breaking-change report when its
// `buildOne(stream, current)` is invoked. The baseline corpus is
// captured at construction; the candidate corpus is supplied per
// call. Registering this generator into the process-global registry
// under the id "breaking-changes" lets a caller look it up the same
// way they would any built-in generator.
class BreakingChangesGenerator final : public Generator
{
public:
    explicit BreakingChangesGenerator(Corpus const& baseline)
        : baseline_(&baseline)
    {}

    std::string_view id()           const noexcept override;
    std::string_view displayName()  const noexcept override;
    std::string_view fileExtension()const noexcept override;

    Expected<void>
    buildOne(std::ostream& os, Corpus const& current) const override;

private:
    Corpus const* baseline_;
};
// end::generator-class[]

} // namespace mrdocs::example

#endif
