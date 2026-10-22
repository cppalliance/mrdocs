//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Metadata/Finalizers/AnchorFinalizer.hpp>
#include <mrdocs/Support/Radix.hpp>
#include <mrdocs/Support/Validate.hpp>
#include <mrdocs/ADT/UnorderedStringMap.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Support/String/String.hpp>
#include <mrdocs/Support/TypeTraits/Concepts.hpp>
#include <mrdocs/Support/TypeTraits/TypeTraits.hpp>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <map>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/xxhash.h>

namespace mrdocs {
namespace {

std::string
getUnnamedInfoName(Symbol const& I)
{
    // All valid c++ identifiers begin with
    // an underscore or alphabetic character,
    // so a numeric prefix ensures no conflicts
    if (I.isFunction() || I.isOverloads())
    {
        static
        constexpr
        std::string_view
        func_reserved[] = {
            "2function",
            "2constructor",
            "2conversion",
            "2destructor"
        };
        std::size_t func_idx = 0;
        if (auto const* FI = I.asFunctionPtr())
        {
            // don't use the reserved prefix for overloaded operators
            if(FI->FuncClass == FunctionClass::Normal &&
                FI->OverloadedOperator != OperatorKind::None)
            {
                return std::string(getSafeOperatorName(
                    FI->OverloadedOperator, true));
            }
            func_idx = to_underlying(FI->FuncClass);
        }
        if (auto const* FI = I.asOverloadsPtr())
        {
            // don't use the reserved prefix for overloaded operators
            if(FI->FuncClass == FunctionClass::Normal &&
                FI->OverloadedOperator != OperatorKind::None)
            {
                return std::string(getSafeOperatorName(
                    FI->OverloadedOperator, true));
            }
            func_idx = to_underlying(FI->FuncClass);
        }
        MRDOCS_ASSERT(func_idx < std::size(func_reserved));
        return std::string(func_reserved[func_idx]);
    }

    std::size_t const idx = to_underlying(I.Kind) - 1;
    std::string res = "_";
    // push idx as two digits
    res.push_back(static_cast<char>('0' + (idx / 10)));
    res.push_back(static_cast<char>('0' + (idx % 10)));
    // push the name of the kind as kebab-case
    auto const kindStr = std::string(toString(I.Kind));
    res += toKebabCase(kindStr);
    return res;
}

// A member of a scope, with the anchor it is getting.
struct ScopeEntry
{
    Symbol const* symbol;

    // The member's name as an anchor may hold it, before disambiguation.
    std::string raw;

    std::string anchor;

    // Whether the member keeps the plain name, so that no other member
    // takes it from it. The global namespace has it from the start.
    bool keepsPlainName = false;
};

// The kinds in the order in which they claim a name that members of
// different kinds share, with the word that sets each one apart when a
// kind that comes earlier keeps the plain name. Each word is a keyword or
// starts with a digit, so it is never the name of a member: in a
// single-page fragment, the anchor of a member follows the anchor of its
// parent after a `-`. Every record has the word `class`, whatever its
// key, so that a `struct` that becomes a `class` keeps its anchor.
struct RankedKind
{
    SymbolKind kind;
    std::string_view word;
};

constexpr RankedKind kindRanking[] = {
    {SymbolKind::Namespace, "namespace"},
    {SymbolKind::Record, "class"},
    {SymbolKind::Enum, "enum"},
    {SymbolKind::Typedef, "typedef"},
    {SymbolKind::Concept, "concept"},
    {SymbolKind::Variable, "2variable"},
    {SymbolKind::Overloads, "2function"},
    {SymbolKind::Guide, "2guide"},
    {SymbolKind::Using, "using"},
    {SymbolKind::NamespaceAlias, "namespace"},
    {SymbolKind::EnumConstant, "2enumerator"},
    {SymbolKind::Macro, "2macro"},
};

// The row of a member's kind in `kindRanking`. A function ranks with the
// overload sets, since it shares its name with its set.
std::size_t
rankOf(Symbol const& I)
{
    SymbolKind const kind = I.isFunction() ? SymbolKind::Overloads : I.Kind;
    RankedKind const* const found =
        std::ranges::find(kindRanking, kind, &RankedKind::kind);
    MRDOCS_ASSERT(found != std::ranges::end(kindRanking));
    return static_cast<std::size_t>(found - std::ranges::begin(kindRanking));
}

std::string_view
kindWord(Symbol const& I)
{
    return kindRanking[rankOf(I)].word;
}

// Append a word to the text of an anchor, after a `-` unless the text is
// empty.
void
appendWord(std::string& text, std::string_view word)
{
    if (!text.empty())
    {
        text.push_back('-');
    }
    text.append(word);
}

bool
isIdentifierChar(char c)
{
    return c == '_' || isAlphaNumeric(c);
}

// Append the word the spelling of a type starts with, if it starts with
// one, and return how many characters of the spelling it took.
std::size_t
appendLeadingToken(std::string& text, std::string_view spelling)
{
    // Punctuation that the spelling of a type keeps in an anchor, as a
    // word. Longer spellings come first, so that `&&` is not taken for
    // two `&`.
    using Spelled = std::pair<std::string_view, std::string_view>;
    static constexpr Spelled spelledPunctuation[] = {
        {"...", "pack"},
        {"&&", "rref"},
        {"&", "ref"},
        {"*", "ptr"},
    };
    std::size_t used = 1;
    if (isIdentifierChar(spelling.front()))
    {
        used = static_cast<std::size_t>(
            std::ranges::find_if_not(spelling, isIdentifierChar) -
            spelling.begin());
        appendWord(text, spelling.substr(0, used));
    }
    else if (Spelled const* const match = std::ranges::find_if(
                 spelledPunctuation,
                 [spelling](Spelled const& p)
                 { return spelling.starts_with(p.first); });
             match != std::ranges::end(spelledPunctuation))
    {
        appendWord(text, match->second);
        used = match->first.size();
    }
    return used;
}

// Append the spelling of a type or of a template argument, in the words
// an anchor may hold: `const char*` becomes `const-char-ptr`.
void
appendSpelling(std::string& text, std::string_view spelling)
{
    std::size_t i = 0;
    while (i < spelling.size())
    {
        i += appendLeadingToken(text, spelling.substr(i));
    }
}

// An anchor takes at most this many characters of a signature, or of the
// type a conversion function converts to. A longer one is replaced by a
// hash of it.
constexpr std::size_t maxSignatureLength = 64;

// The spelled signature, or, if it is longer than an anchor takes, `h`
// followed by the first eight hex digits of its xxh3 hash, which is the
// same on every platform.
std::string
shortSignature(std::string text)
{
    std::string result = std::move(text);
    if (result.size() > maxSignatureLength)
    {
        std::uint64_t const hash = llvm::xxh3_64bits(llvm::StringRef(result));
        result = std::format("h{:08x}", hash >> 32);
    }
    return result;
}

// The parts that tell apart members of one kind by a list, such as their
// parameters: the number of its elements, then their spelling.
std::vector<std::string>
countAndSpelling(std::size_t count, std::string spelling)
{
    return {std::to_string(count), shortSignature(std::move(spelling))};
}

void
appendParameters(std::string& text, std::vector<Param> const& params)
{
    for (Param const& P : params)
    {
        appendSpelling(text, toString(*P.Type));
    }
}

void
appendQualifiers(std::string& text, FunctionSymbol const& F)
{
    std::pair<bool, std::string_view> const qualifiers[] = {
        {F.IsVariadic, "variadic"},
        {F.IsConst, "const"},
        {F.IsVolatile, "volatile"},
        {F.RefQualifier == ReferenceKind::LValue, "ref"},
        {F.RefQualifier == ReferenceKind::RValue, "rref"},
    };
    for (auto const& [present, word] : qualifiers)
    {
        if (present)
        {
            appendWord(text, word);
        }
    }
}

// The parts of a function's signature that can tell it apart from its
// overloads, from the coarsest to the finest: the number of parameters,
// the parameter types with the qualifiers, and the number of template
// parameters.
std::vector<std::string>
functionParts(FunctionSymbol const& F)
{
    std::string signature;
    appendParameters(signature, F.Params);
    appendQualifiers(signature, F);
    std::vector<std::string> parts =
        countAndSpelling(F.Params.size(), std::move(signature));
    parts.push_back(F.Template
        ? "t" + std::to_string(F.Template->Params.size())
        : std::string());
    return parts;
}

// The first function of an overload set, if the corpus has it.
FunctionSymbol const*
firstFunction(OverloadsSymbol const& O, Corpus const& corpus)
{
    Symbol const* const first =
        O.Members.empty() ? nullptr : corpus.find(O.Members.front());
    return first ? first->asFunctionPtr() : nullptr;
}

bool
isStaticSet(OverloadsSymbol const& O, Corpus const& corpus)
{
    FunctionSymbol const* const F = firstFunction(O, corpus);
    return F && F->StorageClass == StorageClassKind::Static;
}

// The type that a conversion function converts to.
Type const&
conversionTarget(FunctionSymbol const& F, Corpus const&)
{
    return *F.ReturnType;
}

// The type that the functions of an overload set of conversion functions
// convert to. A set gathers the functions of one name, so they all spell
// it alike, and the type of the first one stands for all of them.
Type const&
conversionTarget(OverloadsSymbol const& O, Corpus const& corpus)
{
    FunctionSymbol const* const F = firstFunction(O, corpus);
    return F ? *F->ReturnType : *O.ReturnType;
}

// The name of a conversion function, or of an overload set of them, as an
// anchor holds it: the reserved name followed by the spelling of the type
// they convert to (`2conversion-int`), or by a hash of the spelling when
// it is longer than an anchor takes, so that conversions to different
// types never share a name.
template <class FunctionLike>
std::string
conversionName(FunctionLike const& F, Corpus const& corpus)
{
    std::string target;
    appendSpelling(target, toString(conversionTarget(F, corpus)));
    std::string name = getUnnamedInfoName(F);
    appendWord(name, shortSignature(std::move(target)));
    return name;
}

// The part that tells apart overload sets of one name, which a class
// splits by access and by whether the functions are static: the access,
// followed by `static` for static functions.
std::vector<std::string>
overloadSetParts(OverloadsSymbol const& O, Corpus const& corpus)
{
    std::string words(toString(O.Access));
    if (isStaticSet(O, corpus))
    {
        appendWord(words, "static");
    }
    return {std::move(words)};
}

// The part that tells apart using-declarations of one name: the class or
// namespace each one names it in, which follows the word `using`.
std::vector<std::string>
usingParts(UsingSymbol const& U)
{
    std::vector<std::string> parts;
    Name const& introduced = *U.IntroducedName;
    if (introduced.Prefix)
    {
        std::string qualifier;
        appendSpelling(qualifier, (*introduced.Prefix)->Identifier);
        parts.push_back(std::move(qualifier));
    }
    return parts;
}

// The parts of a specialization that can tell it apart from the others:
// the number of template arguments, and the arguments.
std::vector<std::string>
templateArgumentParts(TemplateInfo const& T)
{
    std::string arguments;
    for (Polymorphic<TArg> const& A : T.Args)
    {
        appendSpelling(arguments, toString(*A));
    }
    return countAndSpelling(T.Args.size(), std::move(arguments));
}

std::vector<std::string>
guideParts(GuideSymbol const& G)
{
    std::string parameters;
    appendParameters(parameters, G.Params);
    return countAndSpelling(G.Params.size(), std::move(parameters));
}

std::vector<std::string>
signatureParts(Symbol const& I, Corpus const& corpus)
{
    return visit(I, [&corpus]<class SymbolTy>(SymbolTy const& S)
        -> std::vector<std::string>
    {
        std::vector<std::string> parts;
        if constexpr (std::same_as<SymbolTy, FunctionSymbol>)
        {
            parts = functionParts(S);
        }
        else if constexpr (std::same_as<SymbolTy, OverloadsSymbol>)
        {
            parts = overloadSetParts(S, corpus);
        }
        else if constexpr (std::same_as<SymbolTy, GuideSymbol>)
        {
            parts = guideParts(S);
        }
        else if constexpr (std::same_as<SymbolTy, UsingSymbol>)
        {
            parts = usingParts(S);
        }
        else if constexpr (requires { S.Template; })
        {
            if (S.Template && !S.Template->Args.empty())
            {
                parts = templateArgumentParts(*S.Template);
            }
        }
        return parts;
    });
}

// How strongly a symbol that is not a function claims the plain name: a
// primary template most, a symbol that is no template less, and a
// specialization not at all.
int
templateClaim(Symbol const& I)
{
    return visit(I, []<class SymbolTy>(SymbolTy const& S)
    {
        int claim = 1;
        if constexpr (requires { S.Template; })
        {
            if (S.Template)
            {
                claim = S.Template->specializationKind() ==
                    TemplateSpecKind::Primary ? 2 : 0;
            }
        }
        return claim;
    });
}

bool
hasPlainCall(FunctionSymbol const& F)
{
    return F.Params.empty() && !F.IsVariadic && !F.IsConst &&
        !F.IsVolatile && F.RefQualifier == ReferenceKind::None &&
        !F.Template;
}

// How strongly a member claims the plain name by its access: a private
// member least, then a protected one, and a public one, or one with no
// access, such as a member of a namespace, most.
int
accessClaim(AccessKind access)
{
    static constexpr std::pair<AccessKind, int> claimByAccess[] = {
        {AccessKind::None, 2},
        {AccessKind::Public, 2},
        {AccessKind::Protected, 1},
        {AccessKind::Private, 0},
    };
    int claim = 0;
    for (auto const& [kind, value] : claimByAccess)
    {
        if (kind == access)
        {
            claim = value;
        }
    }
    return claim;
}

// How strongly a function that is in no overload set claims the plain
// name: more the more accessible it is; of two functions with the same
// access, more if it is not static, as for overload sets; and then more
// if it is called with no arguments.
int
functionClaim(FunctionSymbol const& F)
{
    int const nonStatic = F.StorageClass == StorageClassKind::Static ? 0 : 1;
    return (accessClaim(F.Access) * 2 + nonStatic) * 2 +
        (hasPlainCall(F) ? 1 : 0);
}

// The weakest claim of an overload set, that of a private set of static
// functions, which is above the claim of every function.
constexpr int lowestSetClaim = 12;

// How strongly an overload set claims the plain name: more than any
// function, and more the more accessible it is; of two sets with the
// same access, the set of non-static functions claims it more.
int
overloadSetClaim(OverloadsSymbol const& O, Corpus const& corpus)
{
    return lowestSetClaim + accessClaim(O.Access) * 2 +
        (isStaticSet(O, corpus) ? 0 : 1);
}

// How strongly a member claims the plain name among the members of its
// kind that share it: an overload set more than any function, and of two
// sets, or of two functions, the more accessible one more; a primary
// template more than a symbol that is no template, and a specialization
// not at all.
int
nameClaim(Symbol const& I, Corpus const& corpus)
{
    int claim = 0;
    if (OverloadsSymbol const* const O = I.asOverloadsPtr())
    {
        claim = overloadSetClaim(*O, corpus);
    }
    else if (FunctionSymbol const* const F = I.asFunctionPtr())
    {
        claim = functionClaim(*F);
    }
    else
    {
        claim = templateClaim(I);
    }
    return claim;
}

// The member with the strongest claim to the name of its kind, unless
// another one has as strong a claim.
ScopeEntry*
findNameHolder(std::vector<ScopeEntry*> const& sameKind, Corpus const& corpus)
{
    int strongest = -1;
    for (ScopeEntry const* const entry : sameKind)
    {
        strongest = std::max(strongest, nameClaim(*entry->symbol, corpus));
    }
    ScopeEntry* holder = nullptr;
    std::size_t holders = 0;
    for (ScopeEntry* const entry : sameKind)
    {
        if (nameClaim(*entry->symbol, corpus) == strongest)
        {
            holder = entry;
            ++holders;
        }
    }
    return holders == 1 ? holder : nullptr;
}

// The member its scope was seeded with as the holder of its name, if one
// of them is: the global namespace, named as one of its own members.
ScopeEntry*
seededHolder(std::vector<ScopeEntry*> const& sameKind)
{
    std::vector<ScopeEntry*>::const_iterator const seeded =
        std::ranges::find(sameKind, true, &ScopeEntry::keepsPlainName);
    return seeded != sameKind.end() ? *seeded : nullptr;
}

// Whether `other` starts with the first `length` parts of `parts`, which
// has at least that many.
bool
samePrefix(
    std::vector<std::string> const& parts,
    std::vector<std::string> const& other,
    std::size_t length)
{
    return other.size() >= length &&
        std::equal(parts.begin(), parts.begin() + length, other.begin());
}

bool
uniquePrefix(
    std::vector<std::vector<std::string>> const& parts,
    std::size_t i,
    std::size_t length)
{
    bool unique = true;
    for (std::size_t j = 0; j < parts.size(); ++j)
    {
        unique = unique && (j == i || !samePrefix(parts[i], parts[j], length));
    }
    return unique;
}

// How many signature parts member `i` needs to tell it apart from the
// others: the fewest that do, or all of them if none do.
std::size_t
partsNeeded(std::vector<std::vector<std::string>> const& parts, std::size_t i)
{
    std::size_t needed = parts[i].size();
    for (std::size_t length = parts[i].size(); length > 0; --length)
    {
        if (uniquePrefix(parts, i, length))
        {
            needed = length;
        }
    }
    return needed;
}

// Whether a part can be the first word a member adds to its name: it
// starts with a digit or with a keyword, like the parts of an overload
// set, so it is never the name of a member.
bool
startsSuffix(std::string_view part)
{
    static constexpr std::string_view keywords[] = {
        "public", "protected", "private", "static"};
    std::string_view const word = part.substr(0, part.find('-'));
    return isDigit(word.front()) ||
        std::ranges::find(keywords, word) != std::ranges::end(keywords);
}

// Append the first `count` parts of a member's signature to its anchor.
// The first word a member adds must be a keyword or start with a digit,
// so a first part that is neither, such as the class a using-declaration
// names its member in, follows the word of the member's kind. A
// using-declaration that adds no part, because it names its member in no
// class (`using ::f;`), still carries the word `using`.
void
appendParts(
    ScopeEntry& entry,
    std::vector<std::string> const& parts,
    std::size_t count)
{
    for (std::size_t k = 0; k < count; ++k)
    {
        if (!parts[k].empty())
        {
            if (entry.anchor == entry.raw && !startsSuffix(parts[k]))
            {
                appendWord(entry.anchor, kindWord(*entry.symbol));
            }
            appendWord(entry.anchor, parts[k]);
        }
    }
    if (entry.anchor == entry.raw && entry.symbol->isUsing())
    {
        appendWord(entry.anchor, kindWord(*entry.symbol));
    }
}

// Add to the anchor of each member the fewest parts of its signature that
// tell it apart from the others.
void
nameBySignature(std::vector<ScopeEntry*> const& entries, Corpus const& corpus)
{
    std::vector<std::vector<std::string>> parts;
    for (ScopeEntry const* const entry : entries)
    {
        parts.push_back(signatureParts(*entry->symbol, corpus));
    }
    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        appendParts(*entries[i], parts[i], partsNeeded(parts, i));
    }
}

// Start the anchor of a member with its name, followed by the word of its
// kind when another kind keeps the plain name.
void
startAnchor(ScopeEntry& entry, bool holdsName, bool ownsPlainName)
{
    entry.anchor = entry.raw;
    if (!ownsPlainName)
    {
        appendWord(entry.anchor, kindWord(*entry.symbol));
    }
    entry.keepsPlainName = holdsName && ownsPlainName;
}

// Name the members of one kind that share a name. The holder of the name
// keeps it, and each of the other members adds the fewest parts of its
// signature that tell it apart.
void
nameKind(
    std::vector<ScopeEntry*> const& sameKind,
    bool ownsPlainName,
    Corpus const& corpus)
{
    ScopeEntry* const seeded = seededHolder(sameKind);
    ScopeEntry* const holder =
        seeded ? seeded : findNameHolder(sameKind, corpus);
    std::vector<ScopeEntry*> others;
    for (ScopeEntry* const entry : sameKind)
    {
        startAnchor(*entry, entry == holder, ownsPlainName);
        if (entry != holder)
        {
            others.push_back(entry);
        }
    }
    nameBySignature(others, corpus);
}

// Name the members of a scope that share a name, ignoring case: the kind
// that ranks first keeps the plain name.
void
nameGroup(std::vector<ScopeEntry*> const& group, Corpus const& corpus)
{
    std::map<std::size_t, std::vector<ScopeEntry*>> byRank;
    for (ScopeEntry* const entry : group)
    {
        byRank[rankOf(*entry->symbol)].push_back(entry);
    }
    bool ownsPlainName = true;
    for (auto const& [rank, sameKind] : byRank)
    {
        nameKind(sameKind, ownsPlainName, corpus);
        ownsPlainName = false;
    }
}

// The members of a scope grouped by a text they hold, ignoring case, in
// the order of the first member of each group: names that differ only in
// case collide as files on a case-insensitive filesystem.
std::vector<std::vector<ScopeEntry*>>
groupIgnoringCase(
    std::vector<ScopeEntry>& scope,
    std::string ScopeEntry::* text)
{
    std::unordered_map<
        std::string_view, std::size_t, CIStringHash, CIStringEqual> groupOf;
    std::vector<std::vector<ScopeEntry*>> groups;
    for (ScopeEntry& entry : scope)
    {
        auto const [found, added] =
            groupOf.try_emplace(std::string_view(entry.*text), groups.size());
        if (added)
        {
            groups.emplace_back();
        }
        groups[found->second].push_back(&entry);
    }
    return groups;
}

std::string
symbolIdText(SymbolID const& id)
{
    return toBase16(std::string_view(
        reinterpret_cast<char const*>(id.data()), id.size()), true);
}

// How many characters of its ID member `i` needs to differ from each of
// the others: one more than the longest prefix it shares with any of them.
std::size_t
idCharsNeeded(std::vector<std::string> const& ids, std::size_t i)
{
    std::size_t needed = 1;
    for (std::size_t j = 0; j < ids.size(); ++j)
    {
        if (j != i)
        {
            std::size_t const common = static_cast<std::size_t>(
                std::ranges::mismatch(ids[i], ids[j]).in1 - ids[i].begin());
            needed = std::max(needed, common + 1);
        }
    }
    return needed;
}

// Separate members whose anchors coincide with a suffix from their symbol
// IDs: `-0`, then as many characters of the ID as tell each member apart
// from the others. The member that keeps the plain name keeps its anchor,
// so at most one member of the set does.
void
appendIdSuffixes(std::vector<ScopeEntry*> const& same)
{
    std::vector<std::string> ids;
    for (ScopeEntry const* const entry : same)
    {
        ids.push_back(symbolIdText(entry->symbol->id));
    }
    for (std::size_t i = 0; i < same.size(); ++i)
    {
        if (!same[i]->keepsPlainName)
        {
            same[i]->anchor += "-0";
            same[i]->anchor.append(ids[i], 0, idCharsNeeded(ids, i));
        }
    }
}

// Give ID suffixes to the members whose anchors coincide, ignoring case,
// and return whether any did.
bool
suffixCoincidingAnchors(std::vector<ScopeEntry>& scope)
{
    bool found = false;
    for (std::vector<ScopeEntry*> const& same :
         groupIgnoringCase(scope, &ScopeEntry::anchor))
    {
        if (same.size() > 1)
        {
            appendIdSuffixes(same);
            found = true;
        }
    }
    return found;
}

// Separate the members whose anchors still coincide, ignoring case: two
// classes whose names differ only in case, say, two overloads that differ
// only in their constraints, or a conversion function whose name, which
// spells the type it converts to, matches the anchor that another member
// reaches with its suffix. An ID suffix can make an anchor coincide with
// another one, so the passes repeat until no two coincide, at most once
// per member.
void
separateDuplicates(std::vector<ScopeEntry>& scope)
{
    bool again = true;
    for (std::size_t pass = 0; again && pass < scope.size(); ++pass)
    {
        again = suffixCoincidingAnchors(scope);
    }
}

// Name the members of one scope. Members that share a name, ignoring
// case, are named together; a member whose name no other one shares
// keeps it.
void
nameScope(std::vector<ScopeEntry>& scope, Corpus const& corpus)
{
    for (std::vector<ScopeEntry*> const& group :
         groupIgnoringCase(scope, &ScopeEntry::raw))
    {
        if (group.size() == 1)
        {
            startAnchor(*group.front(), true, true);
        }
        else
        {
            nameGroup(group, corpus);
        }
    }
    separateDuplicates(scope);
}

// The members a scope names: its own, not the ones it inherits.
constexpr Corpus::TraverseOptions memberTraversal = {.skipInherited = true};

// The unqualified, URL-safe legible name of each symbol, unique among
// its siblings. The names of a scope are settled together, since how a
// member is told apart depends on the other members that share its name;
// `build` then writes each name into the symbol's `Anchor`.
class LegibleNameTable
{
    Corpus const& corpus_;

    std::unordered_map<SymbolID, std::string> anchors_;

public:
    /*  Build the legible names of all symbols in the corpus. The global
        namespace is named as one of its own members that keeps its name,
        `globalName`, so that none of them takes it.
     */
    LegibleNameTable(Corpus const& corpus, std::string_view globalName)
        : corpus_(corpus)
    {
        NamespaceSymbol const& global = corpus_.globalNamespace();
        std::vector<ScopeEntry> scope;
        scope.push_back({
            .symbol = &global,
            .raw = std::string(globalName),
            .keepsPlainName = true});
        nameMembers(global, std::move(scope));
    }

    /*  Visit a symbol and build legible names for its members
     */
    template <class InfoTy>
    void
    operator()(InfoTy const& I)
    {
        if constexpr (SymbolParent<InfoTy> && !std::same_as<InfoTy, OverloadsSymbol>)
        {
            nameMembers(I, {});
        }
    }

    /* Return the disambiguated unqualified legible name for a symbol.

       The name is unique among the symbol's siblings when a scope reached
       the symbol.
     */
    std::string
    getUnqualified(SymbolID const& id) const
    {
        MRDOCS_ASSERT(corpus_.exists(id));
        std::string result;
        if (auto const it = anchors_.find(id); it != anchors_.end())
        {
            result = it->second;
        }
        else
        {
            // A symbol no scope reached: its plain name, which may not
            // be unique among its siblings.
            result = getRawUnqualified(corpus_.get(id));
        }
        return result;
    }

private:
    /*  Name the members of a symbol, then the members of each of them.
     */
    template <class InfoTy>
    void
    nameMembers(InfoTy const& I, std::vector<ScopeEntry> scope)
    {
        collectMembers(I, scope);
        nameScope(scope, corpus_);
        for (ScopeEntry& entry : scope)
        {
            anchors_.emplace(entry.symbol->id, std::move(entry.anchor));
        }
        corpus_.traverse(memberTraversal, I, [this](Symbol const& M)
            {
                visit(M, *this);
            });
    }

    /*  Add the members of a symbol to its scope, and the functions of the
        overload sets among them.
     */
    template <class InfoTy>
    void
    collectMembers(InfoTy const& I, std::vector<ScopeEntry>& scope) const
    {
        corpus_.traverse(memberTraversal, I, [this, &I, &scope](Symbol const& M)
            {
                scope.push_back({&M, getRawUnqualified(M)});

                // Traverse non inherited function overloads inline
                if (OverloadsSymbol const* const MO = M.asOverloadsPtr())
                {
                    corpus_.traverse(*MO, [this, &I, &scope](Symbol const& M2)
                        {
                            // Not inherited in regard to I
                            MRDOCS_CHECK_OR(M2.Parent == I.id);
                            scope.push_back({&M2, getRawUnqualified(M2)});
                        });
                }
            });
    }

    /*  Get the raw unqualified name for a symbol

        This function returns a reference to the original symbol
        name without any disambiguation characters.

        If the symbolhas no name, then a reserved name based
        on the type is returned instead.
     */
    std::string
    getRawUnqualified(SymbolID const& id) const
    {
        Symbol const* I = corpus_.find(id);
        MRDOCS_ASSERT(I);
        return getRawUnqualified(*I);
    }

    /* @copydoc getRawUnqualified(SymbolID const&)
     */
    std::string
    getRawUnqualified(Symbol const& I) const
    {
        MRDOCS_ASSERT(I.id && I.id != SymbolID::global);
        if (I.Name.empty())
        {
            return getUnnamedInfoName(I);
        }

        return visit(I, [&]<typename T>(T const& t) -> std::string
            {
                MRDOCS_ASSERT(!t.Name.empty());
                if constexpr(
                    T::isFunction() ||
                    T::isOverloads())
                {
                    // functions can be explicitly specialized,
                    // and can be overloaded
                    if (t.FuncClass == FunctionClass::Conversion)
                    {
                        return conversionName(t, corpus_);
                    }
                    else if (t.FuncClass != FunctionClass::Normal ||
                        t.OverloadedOperator != OperatorKind::None)
                    {
                        return getUnnamedInfoName(t);
                    }
                    // User-defined literal operators (operator""_x) carry a
                    // double quote in their name, which is not URL-safe; strip
                    // it so the anchor stays a plain identifier (operator_x).
                    if (t.Name.find('"') != std::string::npos)
                    {
                        std::string safe(t.Name);
                        std::erase(safe, '"');
                        return safe;
                    }
                }
                else if constexpr(T::isUsing())
                {
                    if (t.Class == UsingClass::Normal && !t.ShadowDeclarations.empty())
                    {
                        return getRawUnqualified(t.ShadowDeclarations.front());
                    }
                }
                return t.Name;
            });
    }
};

} // (anon)

void
AnchorFinalizer::
build()
{
    // The anchor is the symbol's own unqualified name only: its hashed id
    // when legible names are disabled, otherwise its unique, URL-safe
    // legible name (unique among its siblings). The generator assembles
    // the qualified fragment or multipage path on demand by walking the
    // parent chain and joining these per-symbol anchors.
    if (!config_.legibleNames)
    {
        for (Symbol const& I : corpus_)
        {
            if (Symbol* M = corpus_.find(I.id))
            {
                M->Anchor = toBase16Str(I.id);
            }
        }
        return;
    }

    LegibleNameTable names(corpus_, "index");
    for (Symbol const& I : corpus_)
    {
        if (Symbol* M = corpus_.find(I.id))
        {
            M->Anchor = names.getUnqualified(I.id);
        }
    }
}

} // mrdocs
