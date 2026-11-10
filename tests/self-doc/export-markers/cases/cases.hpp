//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// The cases the export-markers check runs over in the
// `mrdocs-self-doc-export-markers` test. The forms the check accepts come
// first; each class or function after them is wrong in one way, and
// `expected.txt` lists the error the check reports for it. A function
// declared without a body stands for one defined in a source file.

#include <mrdocs/Support/Export.hpp>
#include <string_view>

namespace mrdocs {

//------------------------------------------------
// Accepted
//------------------------------------------------

/** A class with a member in a source file, marked as a whole.
*/
class MRDOCS_DECL MarkedAsWhole
{
    void helper();

public:
    static constexpr int size = 20;
    void run();
    int count() const { return 0; }
};

/** A class with nothing in a source file.
*/
class MRDOCS_VISIBLE HeaderOnly
{
public:
    int count() const { return 0; }
};

/** A class with the marker on the member that needs it.
*/
class MRDOCS_VISIBLE MarkedMembers
{
public:
    MRDOCS_DECL void run();
    int count() const { return 0; }
};

/** A class with a constant of its own type defined after its body.
*/
class MRDOCS_VISIBLE Color
{
public:
    constexpr Color(int r, int g, int b) : r_(r), g_(g), b_(b) {}
    static const Color black;
    MRDOCS_DECL static Color fromHex(std::string_view hex);

private:
    int r_, g_, b_;
};
constexpr inline Color Color::black = Color(0, 0, 0);

/** A static data member defined in a source file.
*/
class MRDOCS_VISIBLE StaticMarked
{
public:
    MRDOCS_DECL static int counter;
};

namespace detail {
// Not public API: no marker.
class Hidden
{
    void run();
};
} // detail

/** A template needs no marker.
*/
template <class T>
class Box
{
    void run();
};

/** A function with its body in a source file.
*/
MRDOCS_DECL void freeMarked();

//------------------------------------------------
// Empty where a marker is needed
//------------------------------------------------

/** Needs MRDOCS_DECL.
*/
class NeedsDecl
{
public:
    void run();
};

/** Needs MRDOCS_VISIBLE.
*/
class NeedsVisible
{
public:
    int count() const { return 0; }
};

/** Needs MRDOCS_DECL on one member.
*/
class MRDOCS_VISIBLE MemberNeedsDecl
{
public:
    MRDOCS_DECL void run();
    void walk();
};

/** Needs MRDOCS_VISIBLE, and MRDOCS_DECL on its one member.
*/
class LateWithoutMarkers
{
public:
    static const LateWithoutMarkers invalid;
    static LateWithoutMarkers parse();
};
constexpr inline LateWithoutMarkers LateWithoutMarkers::invalid = LateWithoutMarkers();

/** Needs MRDOCS_VISIBLE.
*/
class LateMemberMarked
{
public:
    static const LateMemberMarked invalid;
    MRDOCS_DECL static LateMemberMarked parse();
};
constexpr inline LateMemberMarked LateMemberMarked::invalid = LateMemberMarked();

/** Needs MRDOCS_DECL, since a static data member is defined in a source file.
*/
class StaticNeedsDecl
{
public:
    static int counter;
};

/** A free function that needs MRDOCS_DECL.
*/
void freeNeedsDecl();

//------------------------------------------------
// MRDOCS_DECL where another marker or none is needed
//------------------------------------------------

/** Needs MRDOCS_VISIBLE.
*/
class MRDOCS_DECL DeclNeedsVisible
{
public:
    int count() const { return 0; }
};

/** Needs no marker on its member.
*/
class MRDOCS_VISIBLE DeclOnInlineMember
{
public:
    MRDOCS_DECL int count() const { return 0; }
};

/** Needs no marker on its member, which the class marker covers.
*/
class MRDOCS_DECL DeclOnCoveredMember
{
public:
    MRDOCS_DECL void run();
};

/** Needs MRDOCS_VISIBLE and MRDOCS_DECL on its one member.
*/
class MRDOCS_DECL DeclWithLateConstant
{
public:
    static const DeclWithLateConstant invalid;
    static DeclWithLateConstant parse();
};
constexpr inline DeclWithLateConstant DeclWithLateConstant::invalid = DeclWithLateConstant();

/** Needs no marker on its inline member, which the class marker covers.
*/
class MRDOCS_DECL DeclClassInline
{
public:
    void run();
    MRDOCS_DECL int count() const { return 0; }
};

/** Needs no marker on its inline member.
*/
class MRDOCS_VISIBLE LateInline
{
public:
    static const LateInline invalid;
    MRDOCS_DECL static LateInline parse();
    MRDOCS_DECL int count() const { return 0; }
};
constexpr inline LateInline LateInline::invalid = LateInline();

/** Needs only MRDOCS_DECL.
*/
class MRDOCS_DECL MRDOCS_VISIBLE BothNeedsDecl
{
public:
    void run();
};

/** Needs only MRDOCS_VISIBLE.
*/
class MRDOCS_DECL MRDOCS_VISIBLE BothNeedsVisible
{
public:
    int count() const { return 0; }
};

/** A free function that needs no marker.
*/
MRDOCS_DECL inline void freeInlineMarked() {}

//------------------------------------------------
// MRDOCS_VISIBLE where another marker or none is needed. A member or a
// function with MRDOCS_VISIBLE does not parse, so `../rejected/rejected.hpp`
// holds those.
//------------------------------------------------

/** Needs MRDOCS_DECL.
*/
class MRDOCS_VISIBLE VisibleNeedsDecl
{
public:
    void run();
};

namespace detail {
/** Needs no marker.
*/
class MRDOCS_VISIBLE VisibleHidden
{
};
} // detail

/** Needs no marker.
*/
template <class T>
class MRDOCS_VISIBLE VisibleBox
{
};

} // mrdocs
