// Impl fragment of ASTVisitor.cpp (one TU); USR / SymbolID generation.
// Included within `namespace mrdocs {`. Not a standalone header.

namespace {
// Generate the USR for the using-declaration kinds. These need no visitor
// state, so they live in a free helper; returns true and sets `out` when D
// is one of these kinds, false to fall through to the general path.
bool
generateUSRForUsingDecl(clang::Decl const* D, Expected<llvm::SmallString<128>>& out)
{
    llvm::SmallString<128> res;

    if (auto const* NAD = dyn_cast<clang::NamespaceAliasDecl>(D))
    {
        if (clang::index::generateUSRForDecl(cast<clang::Decl>(NAD->getNamespace()), res))
        {
            out = Unexpected(Error("Failed to generate USR")); return true;
        }
        res.append("@NA");
        res.append(NAD->getNameAsString());
        out = res; return true;
    }

    // Handling UsingDecl
    if (auto const* UD = dyn_cast<clang::UsingDecl>(D))
    {
        for (auto const* shadow : UD->shadows())
        {
            if (clang::index::generateUSRForDecl(shadow->getTargetDecl(), res))
            {
                out = Unexpected(Error("Failed to generate USR")); return true;
            }
        }
        res.append("@UDec");
        res.append(UD->getQualifiedNameAsString());
        out = res; return true;
    }

    if (auto const* UD = dyn_cast<clang::UsingDirectiveDecl>(D))
    {
        if (clang::index::generateUSRForDecl(UD->getNominatedNamespace(), res))
        {
            out = Unexpected(Error("Failed to generate USR")); return true;
        }
        res.append("@UDDec");
        res.append(UD->getQualifiedNameAsString());
        out = res; return true;
    }

    // Handling clang::UnresolvedUsingTypenameDecl
    if (auto const* UD = dyn_cast<clang::UnresolvedUsingTypenameDecl>(D))
    {
        if (clang::index::generateUSRForDecl(UD, res))
        {
            out = Unexpected(Error("Failed to generate USR")); return true;
        }
        res.append("@UUTDec");
        res.append(UD->getQualifiedNameAsString());
        out = res; return true;
    }

    // Handling clang::UnresolvedUsingValueDecl
    if (auto const* UD = dyn_cast<clang::UnresolvedUsingValueDecl>(D))
    {
        if (clang::index::generateUSRForDecl(UD, res))
        {
            out = Unexpected(Error("Failed to generate USR")); return true;
        }
        res.append("@UUV");
        res.append(UD->getQualifiedNameAsString());
        out = res; return true;
    }

    // Handling clang::UsingPackDecl
    if (auto const* UD = dyn_cast<clang::UsingPackDecl>(D))
    {
        if (clang::index::generateUSRForDecl(UD, res))
        {
            out = Unexpected(Error("Failed to generate USR")); return true;
        }
        res.append("@UPD");
        res.append(UD->getQualifiedNameAsString());
        out = res; return true;
    }

    // Handling clang::UsingEnumDecl
    if (auto const* UD = dyn_cast<clang::UsingEnumDecl>(D))
    {
        if (clang::index::generateUSRForDecl(UD, res))
        {
            out = Unexpected(Error("Failed to generate USR")); return true;
        }
        res.append("@UED");
        clang::EnumDecl const* ED = UD->getEnumDecl();
        res.append(ED->getQualifiedNameAsString());
        out = res; return true;
    }
    return false;
}
} // namespace

Expected<llvm::SmallString<128>>
ASTVisitor::
generateUSR(clang::Decl const* D) const
{
    MRDOCS_ASSERT(D);
    llvm::SmallString<128> res;

    {
        Expected<llvm::SmallString<128>> usingUSR = llvm::SmallString<128>{};
        if (generateUSRForUsingDecl(D, usingUSR))
        {
            return usingUSR;
        }
    }

    // KRYSTIAN NOTE: clang doesn't currently support
    // generating USRs for friend declarations, so we
    // will improvise until I can merge a patch which
    // adds support for them
    if(auto const* FD = dyn_cast<clang::FriendDecl>(D))
    {
        // first, generate the USR for the containing class
        if (clang::index::generateUSRForDecl(cast<clang::Decl>(FD->getDeclContext()), res))
        {
            return Unexpected(Error("Failed to generate USR"));
        }
        // add a seperator for uniqueness
        res.append("@FD");
        // if the friend declaration names a type,
        // use the USR generator for types
        if (clang::TypeSourceInfo* TSI = FD->getFriendType())
        {
            if (clang::index::generateUSRForType(TSI->getType(), context_, res))
            {
                return Unexpected(Error("Failed to generate USR"));
            }
            return res;
        }
        // otherwise, fallthrough and append the
        // USR of the nominated declaration
        if (!((D = FD->getFriendDecl())))
        {
            return Unexpected(Error("Failed to generate USR"));
        }
    }

    if (clang::index::generateUSRForDecl(D, res))
    {
        return Unexpected(Error("Failed to generate USR"));
    }

    // Clang includes inline namespaces in the USR, but MrDocs treats them as
    // transparent: getParent skips inline namespaces, so a symbol reached
    // through one and the same symbol named past it are a single entity. For
    // example, with `app::inline abi::detail::f`, the definition and a
    // redeclaration or friend written as `app::detail::f` must share a
    // SymbolID. Because an inline namespace can sit anywhere in the path (not
    // just directly above the symbol), strip every inline-namespace component
    // from the USR, wherever it appears, to match that transparent parent chain.
    for (clang::DeclContext const* ctx = D->getDeclContext();
         ctx; ctx = ctx->getParent())
    {
        auto const* ns = dyn_cast<clang::NamespaceDecl>(ctx);
        if (!ns || !ns->isInlineNamespace())
        {
            continue;
        }
        // The USR component this namespace contributes is its own USR minus the
        // enclosing context's USR, e.g. `@N@abi`.
        llvm::SmallString<128> nsUSR;
        if (clang::index::generateUSRForDecl(ns, nsUSR))
        {
            continue;
        }
        llvm::SmallString<128> parentUSR;
        clang::DeclContext const* parent = ns->getParent();
        if (parent && isa<clang::TranslationUnitDecl>(parent))
        {
            parentUSR = "c:";
        }
        else if (auto const* parentDecl = dyn_cast_if_present<clang::Decl>(parent))
        {
            if (clang::index::generateUSRForDecl(parentDecl, parentUSR))
            {
                continue;
            }
        }
        else
        {
            continue;
        }
        if (!nsUSR.str().starts_with(parentUSR.str()))
        {
            continue;
        }
        llvm::StringRef const component = nsUSR.str().substr(parentUSR.size());
        // A namespace component is always followed by another component (a
        // nested namespace, or the symbol's own), so require the trailing `@`
        // to avoid clipping a longer name that merely shares this prefix.
        std::string const needle = component.str() + "@";
        std::string usr(res.str());
        if (std::string::size_type const pos = usr.find(needle);
            pos != std::string::npos)
        {
            usr.erase(pos, component.size());
            res.assign(usr);
        }
    }

    auto const* Described = dyn_cast_if_present<clang::TemplateDecl>(D);
    auto const* Templated = D;
    if (auto const* DT = D->getDescribedTemplate())
    {
        Described = DT;
        if (auto const* TD = DT->getTemplatedDecl())
        {
            Templated = TD;
        }
    }

    if(Described)
    {
        clang::TemplateParameterList const* TPL = Described->getTemplateParameters();
        if(auto const* RC = TPL->getRequiresClause())
        {
            RC = SubstituteConstraintExpressionWithoutSatisfaction(
                sema_, cast<clang::NamedDecl>(isa<clang::FunctionTemplateDecl>(Described) ? Described : Templated), RC);
            if (!RC)
            {
                return Unexpected(Error("Failed to generate USR"));
            }
            clang::ODRHash odr_hash;
            odr_hash.AddStmt(RC);
            res.append("@TPL#");
            res.append(llvm::itostr(odr_hash.CalculateHash()));
        }
    }

    if(auto* FD = dyn_cast<clang::FunctionDecl>(Templated);
        FD && FD->getTrailingRequiresClause())
    {
        clang::Expr const* RC = FD->getTrailingRequiresClause().ConstraintExpr;
        RC = SubstituteConstraintExpressionWithoutSatisfaction(
            sema_, cast<clang::NamedDecl>(Described ? Described : Templated), RC);
        if (!RC)
        {
            return Unexpected(Error("Failed to generate USR"));
        }
        clang::ODRHash odr_hash;
        odr_hash.AddStmt(RC);
        res.append("@TRC#");
        res.append(llvm::itostr(odr_hash.CalculateHash()));
    }

    return res;
}

bool
ASTVisitor::
generateID(
    clang::Decl const* D,
    SymbolID& id) const
{
    if (!D)
    {
        return false;
    }

    if (isa<clang::TranslationUnitDecl>(D))
    {
        id = SymbolID::global;
        return true;
    }

    if (auto exp = generateUSR(D))
    {
        auto h = llvm::SHA1::hash(arrayRefFromStringRef(*exp));
        id = SymbolID(h.data());
        return true;
    }

    return false;
}

SymbolID
ASTVisitor::
generateID(clang::Decl const* D) const
{
    SymbolID id = SymbolID::invalid;
    generateID(D, id);
    return id;
}
