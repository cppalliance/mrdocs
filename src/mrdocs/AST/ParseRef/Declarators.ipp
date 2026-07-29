// Impl fragment of ParseRef.cpp (one TU): declaration-specifier & declarator parsing, included inside class RefParser.

    bool
    parseDeclarationSpecifiers(Optional<Polymorphic<Type>>& dest)
    {
        //static constexpr std::string_view typeQualifiers[] = {
        //    "const", "volatile"
        //};
        static constexpr std::string_view typeModifiers[] = {
            "long", "short", "signed", "unsigned"
        };
        static constexpr std::string_view typeSpecifiers[] = {
            "long", "short", "signed", "unsigned", "const", "volatile"
        };

        // https://en.cppreference.com/w/cpp/language/declarations#Specifiers
        // decl-specifier-seq is a sequence whitespace-separated decl-specifier's
        char const* start = ptr_;
        llvm::SmallVector<std::string_view, 8> specifiers;
        while (true)
        {
            skipWhitespace();
            char const* specStart = ptr_;
            if (peekAny({',', ')', '&'}))
            {
                break;
            }
            if (!parseDeclarationSpecifier(dest))
            {
                // This could be the end of the specifiers, followed
                // by declarators, or an error. We need to check if
                // dest was properly set.
                // If dest was not set, we need to return an error.
                // If we have one of the "long", "short", "signed", "unsigned"
                // specifiers, then we don't have an error because
                // we can later infer the type from these.
                if (!dest &&
                    !contains_any(specifiers, typeModifiers))
                {
                    setError(specStart, "expected declaration specifier");
                    ptr_ = start;
                    return false;
                }
                // Clear the error and let the type modifiers set `dest`
                error_msg_.clear();
                error_pos_ = nullptr;
                break;
            }
            auto const specifierStr =
                trim(std::string_view(specStart, ptr_ - specStart));
            if (contains(typeSpecifiers, specifierStr))
            {
                specifiers.push_back(specifierStr);
            }
            if (!skipWhitespace())
            {
                break;
            }
        }
        if (!dest && specifiers.empty())
        {
            // We need at least one type declarator or specifier
            ptr_ = start;
            return false;
        }

        // Look for conflicting specifiers
        if (specifiers.size() > 1)
        {
            // If we already have a declaration specifier, we need to
            // check if we have a valid sequence of specifiers:
            // - In general, only one type specifier is allowed
            // - "const" and "volatile" can be combined with any type specifier
            //    except itself
            if (contains_n(specifiers, "const", 2))
            {
                setError(start, "multiple 'const' specifiers");
                ptr_ = start;
                return false;
            }

            if (contains_n(specifiers, "volatile", 2))
            {
                setError(start, "multiple 'volatile' specifiers");
                ptr_ = start;
                return false;
            }

            // - "signed" or "unsigned" can be combined with "char", "long", "short", or "int".
            if (contains_n_any(specifiers, {"signed", "unsigned"}, 2))
            {
                setError(start, "multiple 'signed' or 'unsigned' specifiers");
                ptr_ = start;
                return false;
            }

            // - "short" or "long" can be combined with int.
            // - "long" can be combined with "double" and "long"
            // "short" is allowed only once
            // "long" is allowed twice
            if (contains_n(specifiers, "short", 2))
            {
                setError(start, "too many 'short' specifiers");
                ptr_ = start;
                return false;
            }

            if (contains(specifiers, "short") &&
                contains(specifiers, "long"))
            {
                setError(start, "cannot combine 'short' with 'long'");
                ptr_ = start;
                return false;
            }

            if (contains_n(specifiers, "long", 3))
            {
                setError(start, "too many 'long' specifiers");
                ptr_ = start;
                return false;
            }
        }

        // "signed" or "unsigned" can be combined with "char", "long", "short", or "int".
        if (contains_any(specifiers, {"signed", "unsigned"}))
        {
            bool explicitlySigned = contains(specifiers, "signed");
            std::string_view signStr = explicitlySigned ? "signed" : "unsigned";
            // Infer basic fundamental type from "signed" or "unsigned",
            // which is "int"
            if (!dest)
            {
                dest = Polymorphic<Type>(std::in_place_type<NamedType>);
                auto &NTI = (*dest)->asNamed();
                NTI.Name->Identifier = "int";
                NTI.FundamentalType = FundamentalTypeKind::Int;
            }
            // Check if the type is named
            if (!(*dest)->isNamed())
            {
              setError(std::format("expected named type for '{}' specifier",
                                   signStr));
              ptr_ = start;
              return false;
            }
            // Check if the type is "int" or "char"
            auto& namedParam = (*dest)->asNamed();
            if (!namedParam.FundamentalType)
            {
              setError(std::format(
                  "expected fundamental type for '{}' specifier", signStr));
              ptr_ = start;
              return false;
            }
            bool promoted =
                explicitlySigned ?
                    makeSigned(*namedParam.FundamentalType) :
                    makeUnsigned(*namedParam.FundamentalType);
            if (!promoted)
            {
              setError(std::format(
                  "expected 'int' or 'char' for '{}' specifier", signStr));
              ptr_ = start;
              return false;
            }
            // Add the specifier to the type name
            namedParam.Name->Identifier = toString(*namedParam.FundamentalType);
        }

        // - "short" can be combined with int.
        if (contains(specifiers, "short"))
        {
            // Infer basic fundamental type for "short", which is "int"
            if (!dest)
            {
                dest = Polymorphic<Type>(std::in_place_type<NamedType>);
                auto &NTI = (*dest)->asNamed();
                NTI.Name = Polymorphic<Name>(std::in_place_type<IdentifierName>);
                NTI.Name->Identifier = "int";
                NTI.FundamentalType = FundamentalTypeKind::Int;
            }
            // Check if the type is named
            if (!(*dest)->isNamed())
            {
                setError(start, "expected named type for 'short' specifier");
                ptr_ = start;
                return false;
            }

            // Check if the type is "int"
            auto& namedParam = (*dest)->asNamed();
            if (!namedParam.FundamentalType)
            {
                setError(start, "expected fundamental type for 'short' specifier");
                ptr_ = start;
                return false;
            }
            if (bool promoted = makeShort(*namedParam.FundamentalType);
                !promoted)
            {
                setError(start, "expected 'int' for 'short' specifier");
                ptr_ = start;
                return false;
            }
            // Add the specifier to the type name
            namedParam.Name->Identifier = toString(*namedParam.FundamentalType);
        }

        // - "long" can be combined with "int", "double" and "long"
        if (contains(specifiers, "long"))
        {
            // Infer basic fundamental type for "long", which is "int"
            if (!dest)
            {
                dest = Polymorphic<Type>(std::in_place_type<NamedType>);
                auto &NTI = (*dest)->asNamed();
                NTI.Name = Polymorphic<Name>(std::in_place_type<IdentifierName>);
                NTI.Name->Identifier = "int";
                NTI.FundamentalType = FundamentalTypeKind::Int;
            }
            // Check if the type is named
            if (!(*dest)->isNamed())
            {
                setError(start, "expected named type for 'long' specifier");
                ptr_ = start;
                return false;
            }
            auto& namedParam = (*dest)->asNamed();
            if (!namedParam.FundamentalType)
            {
                setError(start, "expected fundamental type for 'long' specifier");
                ptr_ = start;
                return false;
            }
            if (bool const promoted = makeLong(*namedParam.FundamentalType);
                !promoted)
            {
                setError(start, "expected 'int' or 'double' for 'long' specifier");
                ptr_ = start;
                return false;
            }
            if (contains_n(specifiers, "long", 2))
            {
                bool const promoted = makeLong(*namedParam.FundamentalType);
                if (!promoted)
                {
                    setError(start, "expected 'int' or 'double' for 'long long' specifier");
                    ptr_ = start;
                    return false;
                }
            }
            // Add the specifier to the type name
            namedParam.Name->Identifier = toString(*namedParam.FundamentalType);
        }

        // Final check: if dest is still empty, we have an error
        if (!dest)
        {
            ptr_ = start;
            setError("expected parameter type");
            return false;
        }

        // Set cv qualifiers
        (*dest)->IsConst = contains(specifiers, "const");
        (*dest)->IsVolatile = contains(specifiers, "volatile");

        return true;
    }

    bool
    parseDeclarationSpecifier(Optional<Polymorphic<Type>>& dest)
    {
        char const* start = ptr_;

        // Some rules are only valid if dest was initially empty
        auto checkDestWasEmpty = [destWasEmpty=!dest.has_value(), start, this]() {
            if (!destWasEmpty)
            {
                setError(start, "multiple type declaration specifiers");
                ptr_ = start;
                return false;
            }
            return true;
        };

        // https://en.cppreference.com/w/cpp/language/declarations#Specifiers
        // decl-specifier is one of the following specifiers:
        // - typedef specifier (The typedef specifier may not appear in the declaration of a function parameter)
        // - "inline", "virtual", "explicit" (only allowed in function declarations)
        // - "inline" specifier (also allowed in variable declarations)
        // - "friend" specifier (allowed in class and function declarations)
        // - "constexpr" specifier (allowed in variable and function declarations)
        // - "consteval" specifier (allowed in function declarations)
        // - "constinit" specifier, (allowed in variable declarations)
        // - "register", "static", "extern", "mutable", "thread_local" (storage-class specifiers)
        // - Type specifiers (type-specifier-seq) (a sequence of specifiers that names a type):
        //     - cv qualifier
        if (parseAnyKeyword({"const", "volatile"}))
        {
            return true;
        }

        // - simple type specifier: "char", "char8_t", "char16_t", "char32_t",
        // "wchar_t", "bool", "short", "int", "long", "signed", "unsigned",
        // "float", "double", "void"
        if (parseAnyKeyword({"signed", "unsigned", "short", "long"}))
        {
            // These specifiers are handled in parseDeclarationSpecifiers
            // because they can represent or modify the type.
            return true;
        }

        if (parseAnyKeyword(
                { "char",
                  "char8_t",
                  "char16_t",
                  "char32_t",
                  "wchar_t",
                  "bool",
                  "int",
                  "float",
                  "double",
                  "void" }))
        {
            MRDOCS_CHECK_OR(checkDestWasEmpty(), false);

            dest = Polymorphic<Type>(std::in_place_type<NamedType>);
            MRDOCS_ASSERT(dest);
            MRDOCS_ASSERT(!dest->valueless_after_move());
            auto &NTI = (*dest)->asNamed();
            MRDOCS_ASSERT(!NTI.Name.valueless_after_move());
            NTI.Name->Identifier = std::string_view(start, ptr_ - start);
            if (FundamentalTypeKind k;
                fromString(NTI.Name->Identifier, k))
            {
                NTI.FundamentalType = k;
            }
            return true;
        }

        // - "auto"
        if (parseKeyword("auto"))
        {
            MRDOCS_CHECK_OR(checkDestWasEmpty(), false);
            dest = Polymorphic<Type>(std::in_place_type<AutoType>);
            return true;
        }

        if (parseKeyword("decltype"))
        {
            skipWhitespace();
            //     - "decltype(auto)"
            if (parseLiteral("("))
            {
                skipWhitespace();
                if (parseKeyword("auto"))
                {
                    skipWhitespace();
                    if (parseLiteral(")"))
                    {
                        MRDOCS_CHECK_OR(checkDestWasEmpty(), false);
                        dest = Polymorphic<Type>(
                            std::in_place_type<AutoType>);
                        MRDOCS_ASSERT(dest);
                        MRDOCS_ASSERT(!dest->valueless_after_move());
                        static_cast<AutoType&>(**dest).Keyword = AutoKind::
                            DecltypeAuto;
                        return true;
                    }
                }
                // - "decltype(expression)"
                if (!hasMore())
                {
                    setError("expected expression in decltype");
                    ptr_ = start;
                    return false;
                }
                // rewind ptr_ to '('
                --ptr_;
                while (*ptr_ != '(')
                {
                    ptr_--;
                }
                char const* exprStart = ptr_;
                if (parseBalanced("(", ")"))
                {
                    std::string_view expr(exprStart + 1, ptr_ - exprStart - 2);
                    expr = trim(expr);
                    if (expr.empty())
                    {
                        setError("expected expression in decltype");
                        ptr_ = start;
                        return false;
                    }
                    MRDOCS_CHECK_OR(checkDestWasEmpty(), false);
                    dest = Polymorphic<Type>(
                        std::in_place_type<DecltypeType>);
                    MRDOCS_ASSERT(dest);
                    MRDOCS_ASSERT(!dest->valueless_after_move());
                    (*dest)->asDecltype().Operand.Written = expr;
                    return true;
                }
                setError("expected expression in decltype");
                ptr_ = start;
                return false;
            }
            ptr_ = start;
        }

        // - pack indexing specifier (C++26)
        //   auto f(Ts...[0] arg, type_seq<Ts...>)
        //   (unsupported)

        // - "class" specifier
        // - elaborated type specifier
        //     - "class", "struct" or "union" followed by the identifier
        //        optionally qualified
        //     - "class", "struct" or "union" followed by the template
        //        name with template arguments optionally qualified
        // - typename specifier
        if (parseAnyKeyword({"class", "struct", "union", "typename"}))
        {
            skipWhitespace();
            if (parseQualifiedIdentifier(dest, true, true))
            {
                return checkDestWasEmpty();
            }
            ptr_ = start;
        }

        // - "enum" specifier
        // - "enum" followed by the identifier optionally qualified
        if (parseKeyword("enum"))
        {
            skipWhitespace();
            if (parseQualifiedIdentifier(dest, true, false))
            {
                return checkDestWasEmpty();
            }
            ptr_ = start;
        }

        // - previously declared class/enum/typedef name
        // - template name with template arguments optionally qualified
        // - template name without template arguments optionally qualified
        if (parseQualifiedIdentifier(dest, true, true))
        {
            return checkDestWasEmpty();
        }

        ptr_ = start;
        return false;
    }

    bool
    parseBalanced(
        std::string_view const openTag,
        std::string_view const closeTag,
        std::initializer_list<std::string_view> const until = {})
    {
        char const* start = ptr_;
        std::size_t depth = 0;
        while (hasMore())
        {
            if (depth == 0)
            {
                for (std::string_view const& untilTag : until)
                {
                    if (peek(untilTag))
                    {
                        return true;
                    }
                }
            }
            if (parseLiteral(openTag))
            {
                ++depth;
            }
            else if (parseLiteral(closeTag))
            {
                if (depth == 0)
                {
                    setError("unbalanced expression");
                    ptr_ = start;
                    return false;
                }
                --depth;
                if (depth == 0)
                {
                    return true;
                }
            }
            else
            {
                ++ptr_;
            }
        }
        ptr_ = start;
        return false;
    }

    bool
    parseQualifiedIdentifier(
        Optional<Polymorphic<Type>>& dest,
        bool const allowTemplateDisambiguation,
        bool const allowTemplateArguments)
    {
        if (dest)
        {
            MRDOCS_ASSERT(!dest->valueless_after_move());
            setError("type specifier is already set");
            return false;
        }
        // Identifiers separated by "::"
        char const* start = ptr_;
        parseLiteral("::");
        skipWhitespace();
        while (true)
        {
            char const* idStart = ptr_;
            if (!parseIdentifier(allowTemplateDisambiguation))
            {
                break;
            }

            // Populate dest
            auto const idStr = std::string_view(idStart, ptr_ - idStart);
            Optional<Polymorphic<Name>> ParentName
                = dest ?
                     Optional<Polymorphic<Name>>((*dest)->asNamed().Name) :
                     Optional<Polymorphic<Name>>(std::nullopt);
            dest = Polymorphic<Type>(std::in_place_type<NamedType>);
            auto &NTI = (*dest)->asNamed();
            NTI.Name = Polymorphic<Name>(std::in_place_type<IdentifierName>);
            NTI.Name->Identifier = idStr;
            NTI.Name->Prefix = std::move(ParentName);

            // Look for the next "::"
            char const* compStart = ptr_;
            skipWhitespace();
            if (!parseLiteral("::"))
            {
                ptr_ = compStart;
                break;
            }
            skipWhitespace();
        }
        if (!dest)
        {
            ptr_ = start;
            return false;
        }
        if (allowTemplateArguments)
        {
            char const* templateStart = ptr_;
            skipWhitespace();
            if (peek('<'))
            {
                MRDOCS_ASSERT(dest);
                if (!(*dest)->isNamed())
                {
                    setError("expected named type for template arguments");
                    ptr_ = start;
                    return false;
                }
                // Replace the nameinfo with a nameinfo with args
                auto& namedParam = (*dest)->asNamed();
                SpecializationName SNI;
                SNI.Identifier = std::move(namedParam.Name->Identifier);
                SNI.Prefix = std::move(namedParam.Name->Prefix);
                SNI.id = namedParam.Name->id;
                if (!parseTemplateArguments(SNI.TemplateArgs))
                {
                    ptr_ = start;
                    return false;
                }
                namedParam.Name = Polymorphic<Name>(
                    std::in_place_type<SpecializationName>, std::move(SNI));
            }
            else
            {
                ptr_ = templateStart;
            }
        }
        return true;
    }

    enum declarator_property : int {
        // abstract-declarator - it does not need to be named
        abstract = 1,
        // An internal declarator is any declarator other
        // than a reference declarator (there are no pointers
        // or references to references).
        internal_declarator = 2,
        // noptr-declarator - any valid declarator, but if it begins with *,
        // &, or &&, it has to be surrounded by parentheses.
        no_ptr_declarator = 4,
    };

    bool
    parseAbstractDeclarator(Optional<Polymorphic<Type>>& dest)
    {
        return parseDeclarator<abstract>(dest);
    }

    template <int declarator_type>
    bool
    parseDeclarator(Optional<Polymorphic<Type>>& dest)
    {
        char const *start = ptr_;
        if (!parseDeclaratorOrNoPtrDeclarator<declarator_type>(dest))
        {
            // Maybe a valid declarator is parenthesized
            if (peek('(', ' '))
            {
                skipWhitespace();
                MRDOCS_ASSERT(parseLiteral('('));
                if (!parseDeclarator<declarator_type>(dest))
                {
                    ptr_ = start;
                    return false;
                }
                skipWhitespace();
                if (!parseLiteral(')'))
                {
                    setError("expected ')'");
                    ptr_ = start;
                    return false;
                }
                return true;
            }
            // We expected a valid declarator either as the
            // complete declarator or as the noptr-declarator
            // for an array or function
            setError("expected declarator");
            ptr_ = start;
            return false;
        }
        if (!dest)
        {
            setError("no type defined by specifiers and declarator");
            ptr_ = start;
            return false;
        }
        std::size_t suffixLevel = 0;
        auto isNoPtrDeclarator = [&dest, &suffixLevel, this]() {
            if (suffixLevel == 0)
            {
                if ((*dest)->isLValueReference() ||
                    (*dest)->isRValueReference() ||
                    (*dest)->isPointer())
                {
                    return peekBack(')', ' ');
                }
                // Other types don't need to be surrounded by parentheses
                return true;
            }
            // At other levels, we don't need to check for parentheses
            return true;
        };
        while (
            peekAny({'[', '('}, ' ') &&
            isNoPtrDeclarator())
        {
            // The function return type is the type from the specifiers
            // For instance, in "int (*)", we have a pointer to int.
            // But in "int (*)()", where "int (*)" is the noptr-declarator,
            // the pointer wraps the function type: a pointer to function
            // and not a function to pointer.
            // So parsing "int (*)" gives us a pointer to int (the content
            // of dest), but parsing the function should invert this logic,
            // the pointer points to the function and the function returns int.
            // The same logic other elements that have inner types (pointers,
            // arrays, and references).
            // The current inner type of dest is the function return type.
            MRDOCS_ASSERT(dest);
            MRDOCS_ASSERT(!dest->valueless_after_move());
            Optional<Polymorphic<Type> &> inner = innerType(**dest);
            // The more suffixes we have, the more levels of inner types
            // the suffix affects.
            // For instance, in "int (*)[3][6]", we have a pointer to an
            // array of 3 arrays of 6 ints.
            std::size_t curSuffixLevel = suffixLevel;
            while (curSuffixLevel > 0 && inner)
            {
                MRDOCS_ASSERT(!inner->valueless_after_move());
                auto& ref = *inner;
                inner = innerType(*ref);
                --curSuffixLevel;
            }
            char const* parenStart = ptr_;
            if (!parseArrayOrFunctionDeclaratorSuffix<declarator_type>(
                inner ? *inner : *dest))
            {
                setError(parenStart, "expected declarator");
                ptr_ = start;
                return false;
            }
            ++suffixLevel;
        }
        return true;
    }

    template <int declarator_type>
    bool
    parseDeclaratorOrNoPtrDeclarator(Optional<Polymorphic<Type>>& dest)
    {
        static constexpr bool isAbstractDeclarator = static_cast<bool>(declarator_type & abstract);
        static constexpr bool isInternalDeclarator = static_cast<bool>(declarator_type & internal_declarator);

        // https://en.cppreference.com/w/cpp/language/declarations#Declarators
        char const* start = ptr_;

        if (!dest)
        {
            setError("expected parameter type for '...'");
            ptr_ = start;
            return false;
        }

        // The declarator cannot be another specifier keyword
        // that could also be a declarator
        if (peek(isIdentifierContinuation)
            && parseAnyKeyword(
                { "char",
                  "char8_t",
                  "char16_t",
                  "char32_t",
                  "wchar_t",
                  "bool",
                  "int",
                  "float",
                  "double",
                  "void",
                  "auto",
                  "decltype" }))
        {
            setError("expected declarator, not another specifier");
            ptr_ = start;
            return false;
        }

        // Declarators might be surrounded by an arbitrary
        // number of parentheses. We need to keep track
        // of them.
        skipWhitespace();
        std::size_t parenDepth = 0;
        while (parseLiteral("("))
        {
            ++parenDepth;
            skipWhitespace();
        }

        // At the end, we need to consume the closing parentheses
        // in reverse order.
        auto const parseClosingParens = [&]() {
            while (parenDepth > 0)
            {
                skipWhitespace();
                if (!parseLiteral(")"))
                {
                    setError("expected ')'");
                    ptr_ = start;
                    return false;
                }
                --parenDepth;
            }
            return true;
        };

        // https://en.cppreference.com/w/cpp/language/declarations#Declarators
        // declarator - one of the following:
        // (1) The name that is declared:
        //     unqualified-id attr (optional)
        // https://en.cppreference.com/w/cpp/language/identifiers#Names
        char const* idStart = ptr_;
        if (parseIdentifier(false))
        {
            if (parenDepth != 0 &&
                peek(',', ' '))
            {
                // This is a function parameter declaration
                // and this identifier is actually the type
                // of the first parameter
                ptr_ = idStart;
                MRDOCS_ASSERT(rewindUntil('('));
                parenDepth--;
                if (parseFunctionDeclaratorSuffix<declarator_type>(dest))
                {
                    return parseClosingParens();
                }
            }
            else if (!peek(':', ' '))
            {
                // We ignore the name and just return true
                // The current parameter type does not change
                return parseClosingParens();
            }
            // id is qualified-id, so fall through to the next cases
            ptr_ = idStart;
        }

        // (2) A declarator that uses a qualified identifier (qualified-id)
        //     defines or redeclares a previously declared namespace member
        //     or class member.
        //     qualified-id attr (optional)
        // We do not implement this case for function parameters.

        // (3) Parameter pack, only appears in parameter declarations.
        //     ... identifier attr (optional)
        // https://en.cppreference.com/w/cpp/language/pack
        if (parseLiteral("..."))
        {
            skipWhitespace();
            parseIdentifier(false);
            (*dest)->IsPackExpansion = true;
            return parseClosingParens();
        }

        // (4) Pointer declarator: the declaration `S * D`; declares declarator
        //     `D` as a pointer to the type determined by decl-specifier-seq `S`.
        //     * attr (optional) cv (optional) declarator
        // https://en.cppreference.com/w/cpp/language/pointer
        if (parseLiteral("*"))
        {
            if ((*dest)->isLValueReference() ||
                (*dest)->isRValueReference())
            {
                setError("pointer to reference not allowed");
                ptr_ = start;
                return false;
            }

            // Change current type to pointer type
            PointerType PTI;
            PTI.PointeeType = std::move(*dest);
            dest = Polymorphic<Type>(std::move(PTI));

            skipWhitespace();
            // cv is a sequence of const and volatile qualifiers,
            // where either qualifier may appear at most once
            // in the sequence.
            parseCV((*dest)->IsConst, (*dest)->IsVolatile);
            // Parse the next declarator, potentially wrapping
            // the destination type in another type
            // If this declarator is abstract, the pointer
            // declarator is also abstract.
            if (constexpr int nextDeclaratorType = (declarator_type & abstract)
                                                   | internal_declarator;
                !parseDeclarator<nextDeclaratorType>(dest))
            {
                setError("expected declarator after pointer");
                ptr_ = start;
                return false;
            }
            return parseClosingParens();
        }

        // (5) Pointer to member declaration: the declaration `S C::* D`;
        //     declares `D` as a pointer to member of `C` of type determined
        //     by decl-specifier-seq `S`. nested-name-specifier is a
        //     sequence of names and scope resolution operators ::
        //     nested-name-specifier * attr (optional) cv (optional) declarator
        // https://en.cppreference.com/w/cpp/language/pointer
        if (parseNestedNameSpecifier())
        {
            char const* NNSEnd = ptr_;
            skipWhitespace();
            if (!parseLiteral("*"))
            {
                ptr_ = start;
                return false;
            }

            if constexpr (isInternalDeclarator)
            {
                setError("pointer to member declarator not allowed in this context");
                ptr_ = start;
                return false;
            }

            // Assemble the parent type for the NNS
            NamedType ParentType;
            auto NNSString = std::string_view(start, NNSEnd - start);
            IdentifierName NNS;
            auto const NNSRange = llvm::split(NNSString, "::");
            MRDOCS_ASSERT(!NNSRange.empty());
            auto NNSIt = NNSRange.begin();
            std::string_view unqualID = *NNSIt;
            NNS.Identifier = std::string(unqualID);
            ++NNSIt;
            while (NNSIt != NNSRange.end())
            {
                unqualID = *NNSIt;
                if (unqualID.empty())
                {
                    break;
                }
                IdentifierName NewNNS;
                NewNNS.Identifier = std::string(unqualID);
                NewNNS.Prefix = Polymorphic<Name>(std::move(NNS));
                NNS = NewNNS;
                ++NNSIt;
            }
            ParentType.Name = Polymorphic<Name>(std::move(NNS));

            // Change current type to member pointer type
            MemberPointerType MPTI;
            MPTI.PointeeType = Polymorphic<Type>(std::move(*dest));
            MPTI.ParentType = Polymorphic<Type>(std::move(ParentType));
            dest = Polymorphic<Type>(std::move(MPTI));

            skipWhitespace();
            // cv is a sequence of const and volatile qualifiers,
            // where either qualifier may appear at most once
            // in the sequence.
            parseCV((*dest)->IsConst, (*dest)->IsVolatile);
            parseIdentifier(false);
            // We ignore the name and just return true
            return parseClosingParens();
        }

        // (6) Lvalue reference declarator: the declaration `S & D`; declares
        //     `D` as an lvalue reference to the type determined by
        //     decl-specifier-seq `S`.
        //     & attr (optional) declarator
        // https://en.cppreference.com/w/cpp/language/reference
        if (parseLiteral("&"))
        {
            if constexpr (isInternalDeclarator)
            {
                setError("lvalue reference to pointer not allowed");
                ptr_ = start;
                return false;
            }

            skipWhitespace();

            // (7) Rvalue reference declarator: the declaration `S && D`;
            //     declares D as an rvalue reference to the type determined
            //     by decl-specifier-seq `S`.
            //     && attr (optional) declarator

            // Change current type to reference type
            if (bool const isRValue = parseLiteral("&");
                !isRValue)
            {
                LValueReferenceType RTI;
                RTI.PointeeType = std::move(*dest);
                dest = Polymorphic<Type>(std::move(RTI));
            }
            else
            {
                RValueReferenceType RTI;
                RTI.PointeeType = std::move(*dest);
                dest = Polymorphic<Type>(std::move(RTI));
            }

            skipWhitespace();

            // Parse the next declarator, potentially wrapping
            // the destination type in another type
            static constexpr int nextDeclaratorType
                = (declarator_type & abstract) | internal_declarator;
            if (!parseDeclarator<nextDeclaratorType>(dest))
            {
                setError("expected declarator after reference");
                ptr_ = start;
                return false;
            }

            return parseClosingParens();
        }

        // (8-9) Array and function declarators are handled in a separate
        // function
        parenDepth = 0;
        ptr_ = start;
        MRDOCS_ASSERT(dest);
        if (parseArrayOrFunctionDeclaratorSuffix<declarator_type>(*dest))
        {
            return true;
        }

        // (10) An abstract declarator can also be an empty string, which
        // is equivalent to unnamed (1) unqualified-id.
        if constexpr (isAbstractDeclarator)
        {
            return parseClosingParens();
        }
        else
        {
            setError("expected a concrete declarator");
            ptr_ = start;
            return false;
        }
    }
