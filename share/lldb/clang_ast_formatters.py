#
# Copyright (c) 2025 alandefreitas (alandefreitas@gmail.com)
#
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt
#

import lldb
import re


# ================================================================
# Small utilities
# ================================================================
def _ctx(v):
    try:
        return v.Dereference() if v.TypeIsPointerType() else v
    except Exception:
        return v

def _eval_str(scope, code):
    opts = lldb.SBExpressionOptions()
    opts.SetLanguage(lldb.eLanguageTypeC_plus_plus)
    opts.SetCoerceResultToId(False)
    r = scope.EvaluateExpression(code, opts)
    if r and r.GetError() and r.GetError().Success():
        s = r.GetSummary() or r.GetValue()
        return s.strip('"') if s else None
    return None

def _strip_ptr_ref(t):
    # SBType -> SBType (no pointers/refs)
    try:
        # Drop references first
        dt = t.GetDereferencedType()
        if dt and dt.IsValid():
            t = dt
        # Drop pointers next
        while t.IsPointerType():
            t = t.GetPointeeType()
    except Exception:
        pass
    return t

def _humanize_kind(kind: str) -> str:
    # Insert spaces at transitions: a|A, A|A a, digit|A; then lowercase.
    if not kind:
        return "decl"
    s = re.sub(r'(?<=[a-z0-9])(?=[A-Z])', ' ', kind)
    s = re.sub(r'(?<=[A-Z])(?=[A-Z][a-z])', ' ', s)
    return s.lower()

def _is_derived_from(sbtype: lldb.SBType, qualified_basename: str) -> bool:
    """Walk base classes to see if sbtype derives from qualified_basename."""
    try:
        t = _strip_ptr_ref(sbtype)
        seen = set()
        q = [t]
        steps = 0
        while q and steps < 128:
            steps += 1
            cur = q.pop()
            n = cur.GetName() or ""
            if n == qualified_basename or n.endswith("::" + qualified_basename.split("::")[-1]):
                # Accept exact or namespaced matches
                if n.endswith(qualified_basename):
                    return True
            num = cur.GetNumberOfDirectBaseClasses()
            for i in range(num):
                m = cur.GetDirectBaseClassAtIndex(i)  # SBTypeMember
                bt = m.GetType()
                if bt and bt.IsValid():
                    bn = bt.GetName() or ""
                    if bn not in seen:
                        seen.add(bn)
                        q.append(bt)
    except Exception:
        pass
    return False


# ================================================================
# Decl recognizers & summaries
# ================================================================
def is_clang_nameddecl(sbtype, _dict):
    t = _strip_ptr_ref(sbtype)
    n = t.GetName() or ""
    if not n.startswith("clang::"):
        return False
    return _is_derived_from(t, "clang::NamedDecl")

def is_clang_decl_not_named(sbtype, _dict):
    t = _strip_ptr_ref(sbtype)
    n = t.GetName() or ""
    if not n.startswith("clang::"):
        return False
    # Must be a Decl, but NOT a NamedDecl
    return _is_derived_from(t, "clang::Decl") and not _is_derived_from(t, "clang::NamedDecl")

def _kind_from_type(valobj):
    t = _strip_ptr_ref(valobj.GetType())
    name = t.GetName() or ""
    m = re.search(r'clang::([A-Za-z_0-9]+)Decl', name)
    return m.group(1) if m else "Decl"

# ================================================================
# Decl Summaries
# ================================================================

# NamedDecl (qualified name + humanized kind)
def NamedDecl_summary(valobj, _dict):
    o = _ctx(valobj)

    # Qualified name first
    q = _eval_str(o, "static_cast<const ::clang::NamedDecl*>(this)->getNameAsString()")
    if not q:
        q = _eval_str(o, "static_cast<const ::clang::NamedDecl*>(this)->getNameAsString()")

    # Anonymous namespace friendly label (for the rare case q is still empty)
    if not q:
        anon = _eval_str(
            o,
            "(static_cast<const ::clang::NamespaceDecl*>(this)->isAnonymousNamespace() "
            "? std::string(\"(anonymous namespace)\") : std::string())"
        )
        if anon:
            q = anon

    # Decl kind → humanized lowercase
    kind = _eval_str(o, "std::string(getDeclKindName())") or _kind_from_type(valobj)
    hkind = _humanize_kind(kind)

    return f"{q} ({hkind})" if q else f"<unnamed> ({hkind})"

# Non-NamedDecl (unnamed) → "<unnamed> (humanized kind)"
def UnnamedDecl_summary(valobj, _dict):
    o = _ctx(valobj)
    kind = _eval_str(o, "std::string(getDeclKindName())") or _kind_from_type(valobj)
    hkind = _humanize_kind(kind)
    return f"<unnamed> ({hkind})"

# ================================================================
# Comment recognizer & frame-aware summary (actual source text)
# ================================================================
def is_clang_comment(sbtype, _dict):
    try:
        n = _strip_ptr_ref(sbtype).GetName() or ""
        return n.startswith("clang::comments::")
    except Exception:
        return False

def _frame():
    try:
        tgt = lldb.debugger.GetSelectedTarget()
        if not tgt: return None
        proc = tgt.GetProcess()
        if not proc: return None
        thr = proc.GetSelectedThread()
        if not thr: return None
        return thr.GetSelectedFrame()
    except Exception:
        return None

def _frame_eval_str(code: str):
    f = _frame()
    if not f:
        return None
    opts = lldb.SBExpressionOptions()
    opts.SetLanguage(lldb.eLanguageTypeC_plus_plus)
    opts.SetCoerceResultToId(False)
    r = f.EvaluateExpression(code, opts)
    if r and r.GetError() and r.GetError().Success():
        s = r.GetSummary() or r.GetValue()
        return s.strip('"') if s else None
    return None

def _comments_ptr_expr(valobj):
    try:
        addr = valobj.GetValueAsUnsigned(0)
        if addr:
            return f"(const ::clang::comments::Comment*)0x{addr:x}"
    except Exception:
        pass
    return None

def _astctx_candidates_from_frame():
    # Order these to match your codebase first
    return [
        "(&this->ctx_)",   # common in visitors
        "(&ctx_)",
        "(&Ctx)",
        "(&context)",
        "(&Context)",
        "(&ASTCtx)",
    ]

def _shorten_one_line(s: str, maxlen=160) -> str:
    if not s:
        return None
    s = " ".join(s.replace("\r", "\n").split())
    return (s[: maxlen - 3] + "...") if len(s) > maxlen else s

def Comment_summary(valobj, _dict):
    """
    Show the exact source slice of the comment (single line), if we can obtain
    an ASTContext from the current frame. Otherwise, fall back to a type label.
    """
    cptr = _comments_ptr_expr(valobj)
    if cptr:
        text_tmpl = (
            "{ "
            f"  auto C = {cptr};"
            "  ::clang::SourceRange R = C->getSourceRange();"
            "  const ::clang::ASTContext* ACTX = ASTCTX_EXPR;"
            "  if (!ACTX) return std::string();"
            "  const auto& SM = ACTX->getSourceManager();"
            "  return std::string(::clang::Lexer::getSourceText("
            "    ::clang::CharSourceRange::getTokenRange(R), SM, ACTX->getLangOpts()));"
            "}"
        )
        for cand in _astctx_candidates_from_frame():
            code = text_tmpl.replace("ASTCTX_EXPR", cand)
            s = _frame_eval_str(code)
            if s:
                one = _shorten_one_line(s)
                if one:
                    return one  # just the text; no extra suffix

    # Fallback: readable type label (keeps something useful when ACTX is unreachable)
    tname = (_strip_ptr_ref(valobj.GetType()).GetName() or "").rsplit("::", 1)[-1]
    base = _humanize_kind(tname.replace("Comment", "")) or "comment"
    return f"{base} (comment)"


# ================================================================
# Registration
# ================================================================
def __lldb_init_module(debugger, _dict):
    # Decl summaries
    debugger.HandleCommand(
        'type summary add '
        '--python-function clang_ast_formatters.NamedDecl_summary '
        '--recognizer-function clang_ast_formatters.is_clang_nameddecl'
    )
    debugger.HandleCommand(
        'type summary add '
        '--python-function clang_ast_formatters.UnnamedDecl_summary '
        '--recognizer-function clang_ast_formatters.is_clang_decl_not_named'
    )

    # Comments summary (no synthetic; members remain visible)
    debugger.HandleCommand(
        'type summary add '
        '--python-function clang_ast_formatters.Comment_summary '
        '--recognizer-function clang_ast_formatters.is_clang_comment'
    )

    print("[clang-ast] Decl summaries + comments source-text summaries registered.")
