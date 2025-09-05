#
# Copyright (c) 2025 alandefreitas (alandefreitas@gmail.com)
#
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt
#

import lldb
import re


# ---------- tiny helpers ----------
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

def _strip_ptr_ref(t):  # SBType -> SBType (no pointers/refs)
    try:
        # Drop references if possible
        dt = t.GetDereferencedType()
        if dt and dt.IsValid():  # only valid for refs
            t = dt
        # Drop pointers
        while t.IsPointerType():
            t = t.GetPointeeType()
    except Exception:
        pass
    return t

def _kind_from_type(valobj):
    t = _strip_ptr_ref(valobj.GetType())
    name = t.GetName() or ""
    m = re.search(r'clang::([A-Za-z_0-9]+)Decl', name)
    return m.group(1) if m else "Decl"

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


# ---------- recognizers ----------
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


# ---------- summaries ----------
# NamedDecl (qualified name + humanized kind)
def NamedDecl_summary(valobj, _dict):
    o = _ctx(valobj)

    # Qualified name first
    q = _eval_str(o, "static_cast<const ::clang::NamedDecl*>(this)->getQualifiedNameAsString()")
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


def __lldb_init_module(debugger, _dict):
    # One rule for ALL NamedDecl-derived types
    debugger.HandleCommand(
        'type summary add '
        '--python-function clang_ast_formatters.NamedDecl_summary '
        '--recognizer-function clang_ast_formatters.is_clang_nameddecl'
    )
    # And a second rule for other Decl types (non-NamedDecl)
    debugger.HandleCommand(
        'type summary add '
        '--python-function clang_ast_formatters.UnnamedDecl_summary '
        '--recognizer-function clang_ast_formatters.is_clang_decl_not_named'
    )
    print("[clang-ast] NamedDecl and Unnamed Decl summaries registered via recognizers.")
