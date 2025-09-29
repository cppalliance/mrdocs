#
# Copyright (c) 2025 alandefreitas (alandefreitas@gmail.com)
#
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt
#

from __future__ import annotations
import lldb
import re

# =============================================================================
# Core utilities (shared by all providers)
# =============================================================================

def _deref_if_ptr_or_ref(v: lldb.SBValue) -> lldb.SBValue:
    """Return v dereferenced if it is a pointer/reference and deref succeeds; else v unchanged."""
    t = v.GetType()
    if not t or not t.IsValid():
        return v
    if t.IsPointerType() or t.IsReferenceType():
        dv = v.Dereference()
        return dv if dv and dv.IsValid() else v
    return v


def _type_name(t: lldb.SBType) -> str:
    """Best-effort type name ('' if unavailable)."""
    return (t.GetName() or "") if (t and t.IsValid()) else ""


def _get_type_fields(t: lldb.SBType):
    """Iterate SBTypeMember for non-virtual data members/bases (robust to exceptions)."""
    try:
        n = t.GetNumberOfFields()
    except Exception:
        n = 0
    for i in range(n):
        yield t.GetFieldAtIndex(i)


def _get_direct_bases(t: lldb.SBType):
    """Iterate SBTypeMember for direct base classes (robust to exceptions)."""
    try:
        n = t.GetNumberOfDirectBaseClasses()
    except Exception:
        n = 0
    for i in range(n):
        yield t.GetDirectBaseClassAtIndex(i)


def _base_value_from_typemember(parent_v: lldb.SBValue, base_tm: lldb.SBTypeMember) -> lldb.SBValue | None:
    """Materialize a direct base subobject by offset. Returns None on failure."""
    bt = base_tm.GetType()
    if not bt or not bt.IsValid():
        return None
    try:
        off = base_tm.GetOffsetInBytes()
    except Exception:
        off_bits = base_tm.GetOffsetInBits() if hasattr(base_tm, "GetOffsetInBits") else 0
        off = off_bits // 8
    child = parent_v.CreateChildAtOffset(_type_name(bt) or "__base", off, bt)
    return child if child and child.IsValid() else None


def _get_field_value_by_offset(parent_v: lldb.SBValue, field_name: str) -> lldb.SBValue | None:
    """Materialize a named field via type layout offsets (preferred), else try direct lookup."""
    t = parent_v.GetType()
    if t and t.IsValid():
        try:
            n = t.GetNumberOfFields()
        except Exception:
            n = 0
        for i in range(n):
            tm = t.GetFieldAtIndex(i)
            try:
                if tm.IsBaseClass():
                    continue
            except Exception:
                pass
            if (tm.GetName() or "") == field_name:
                ftype = tm.GetType()
                if not (ftype and ftype.IsValid()):
                    break
                try:
                    off = tm.GetOffsetInBytes()
                except Exception:
                    off_bits = tm.GetOffsetInBits() if hasattr(tm, "GetOffsetInBits") else 0
                    off = off_bits // 8
                v = parent_v.CreateChildAtOffset(field_name, off, ftype)
                return v if v and v.IsValid() else None
    v = parent_v.GetChildMemberWithName(field_name)
    return v if v and v.IsValid() else None


def _clean(s: str | None) -> str | None:
    """Strip/normalize a string value; filter out LLDB 'error:' summaries."""
    if not s:
        return None
    s = s.strip()
    if s.startswith("error:"):
        return None
    return s


def _fmt_ptr_addr(p: lldb.SBValue) -> str:
    """Return pointer as 0x<hex> with no leading zeros (LLDB style)."""
    try:
        v = p.GetValueAsUnsigned(0)
        return f"0x{v:x}"
    except Exception:
        s = (p.GetValue() or "").strip()
        if s.startswith(("0x", "0X")):
            h = s[2:].lstrip("0")
            return "0x" + (h if h else "0")
        return s or "0x0"


# =============================================================================
# Child lookup helpers (work across synthetic/non-synthetic)
# =============================================================================

def _strip_prefixes_for_match(nm: str) -> str:
    """Strip ordering prefixes ('NN '), optional decl prefixes ('T::'), and de-dupe suffixes (' #k')."""
    if not nm:
        return nm
    nm = re.sub(r"^\d{2}\s+", "", nm)
    if "::" in nm:
        nm = nm.split("::", 1)[1]
    nm = re.sub(r"\s+#\d+$", "", nm)
    return nm


def _child_by_name_any(v: lldb.SBValue, names: tuple[str, ...]) -> lldb.SBValue | None:
    """Return first child whose name matches any in `names`, searching both synthetic and raw views."""
    if not v or not v.IsValid():
        return None
    views = [v]
    try:
        ns = v.GetNonSyntheticValue()
        if ns and ns.IsValid() and ns.GetID() != v.GetID():
            views.append(ns)
    except Exception:
        pass
    # direct lookup
    for view in views:
        for nm in names:
            ch = view.GetChildMemberWithName(nm)
            if ch and ch.IsValid():
                return ch
    # enumerated children (names may be prefixed by a synthetic)
    for view in views:
        n = view.GetNumChildren()
        for i in range(n):
            ch = view.GetChildAtIndex(i)
            if not (ch and ch.IsValid()):
                continue
            nm = _strip_prefixes_for_match(ch.GetName() or "")
            if nm in names:
                return ch
    return None


def _find_field_recursive_any(v: lldb.SBValue, names: tuple[str, ...]) -> lldb.SBValue | None:
    """Like _child_by_name_any, but recurses into direct bases using type offsets."""
    if not v or not v.IsValid():
        return None
    got = _child_by_name_any(v, names)
    if got and got.IsValid():
        return got
    t = v.GetType()
    if t and t.IsValid():
        # search own fields by offset
        for tm in _get_type_fields(t):
            try:
                if tm.IsBaseClass():
                    continue
            except Exception:
                pass
            if (tm.GetName() or "") in names:
                ft = tm.GetType()
                if ft and ft.IsValid():
                    try:
                        off = tm.GetOffsetInBytes()
                    except Exception:
                        off_bits = tm.GetOffsetInBits() if hasattr(tm, "GetOffsetInBits") else 0
                        off = off_bits // 8
                    ch = v.CreateChildAtOffset(tm.GetName() or "", off, ft)
                    if ch and ch.IsValid():
                        return ch
        # recurse bases
        for btm in _get_direct_bases(t):
            base_v = _base_value_from_typemember(v, btm)
            if base_v and base_v.IsValid():
                got = _find_field_recursive_any(base_v, names)
                if got and got.IsValid():
                    return got
    return None


# =============================================================================
# std::string helpers (simple/robust)
# =============================================================================

def _str_from_std_string(sbv: lldb.SBValue) -> str | None:
    """Extract std::string contents leveraging LLDB’s own summary when available."""
    if not sbv or not sbv.IsValid():
        return None
    s = sbv.GetSummary() or sbv.GetObjectDescription() or sbv.GetValue()
    if not s:
        return None
    s = s.strip()
    if len(s) >= 2 and s[0] == '"' and s[-1] == '"':
        s = s[1:-1]
    return s


# =============================================================================
# Symbol-like: summary + synthetic
# =============================================================================

def _find_std_string_name(v: lldb.SBValue) -> lldb.SBValue | None:
    """Find a std::string-ish member named 'Name' (or common variants) on this object or its bases."""
    candidates = ("Name", "name", "Name_", "name_")
    v = _deref_if_ptr_or_ref(v)
    if not v or not v.IsValid():
        return None
    t = v.GetType()
    if not t or not t.IsValid():
        return None

    # direct members first
    for tm in _get_type_fields(t):
        try:
            if tm.IsBaseClass():
                continue
        except Exception:
            pass
        fname = tm.GetName() or ""
        if fname not in candidates:
            continue
        child = v.GetChildMemberWithName(fname)
        if child and child.IsValid() and _str_from_std_string(child) is not None:
            return child

    # then bases
    for btm in _get_direct_bases(t):
        base_v = _base_value_from_typemember(v, btm)
        if base_v:
            got = _find_std_string_name(base_v)
            if got:
                return got
    return None


def SymbolLikeSummaryProvider(valobj, _dict):
    """Summary for Symbol-like types: show name or '<unnamed>'."""
    try:
        name_val = _find_std_string_name(valobj)
        if not name_val:
            return "<unnamed>"
        s = _str_from_std_string(name_val)
        return s if s else "<unnamed>"
    except Exception as e:
        return f"<summary error: {e}>"


# ---- SymbolID (fixed size blob → hex) ---------------------------------------

_SYMBOLID_HEX_UPPER = False  # set True for uppercase hex

def _read_process_bytes(valobj: lldb.SBValue, addr: int, n: int) -> bytes | None:
    """Read n bytes from process memory at addr; None on failure."""
    if addr == 0 or n <= 0:
        return None
    proc = valobj.GetProcess()
    if not proc or not proc.IsValid():
        return None
    err = lldb.SBError()
    data = proc.ReadMemory(addr, n, err)
    return data if err.Success() and data is not None and len(data) == n else None


def _extract_symbolid_bytes(sym: lldb.SBValue, expected_len: int = 20) -> list[int] | None:
    """Extract byte array from SymbolID::data_ either via children or raw memory."""
    sym = _deref_if_ptr_or_ref(sym)
    if not sym or not sym.IsValid():
        return None
    data_field = _get_field_value_by_offset(sym, "data_")
    if not data_field or not data_field.IsValid():
        return None

    # A: array children
    n = data_field.GetNumChildren()
    if n >= expected_len:
        out = []
        for i in range(expected_len):
            el = data_field.GetChildAtIndex(i)
            if not el or not el.IsValid():
                out = []
                break
            out.append(el.GetValueAsUnsigned(0) & 0xFF)
        if len(out) == expected_len:
            return out

    # B: direct memory
    addr_v = data_field.AddressOf()
    if addr_v and addr_v.IsValid():
        addr = addr_v.GetValueAsUnsigned()
        raw = _read_process_bytes(sym, addr, expected_len)
        if raw is not None:
            return [b for b in raw]
    return None


def SymbolIDSummaryProvider(valobj, _dict):
    """Render SymbolID as hex; special cases: all-FF → <global>, all-00 → std::nullopt."""
    try:
        bs = _extract_symbolid_bytes(valobj, 20)
        if not bs:
            return "<unavailable>"
        if all(b == 0xFF for b in bs):
            return "<global>"
        if all(b == 0x00 for b in bs):
            return "std::nullopt"
        fmt = (lambda b: f"{b:02X}") if _SYMBOLID_HEX_UPPER else (lambda b: f"{b:02x}")
        return "".join(fmt(b) for b in bs)
    except Exception as e:
        return f"<summary error: {e}>"


# ---- Symbol-like synthetic: flatten bases (stable order) ---------------------

_PREFIX_ORDER = True       # prefix children with "NN" to stabilize sort order
_INCLUDE_DECL_TYPE = True  # include "DeclType::" before member name

def _norm_typename(t: lldb.SBType) -> str:
    """Strip common C++ qualifiers and keywords from type name."""
    n = _type_name(t)
    for p in ("class ", "struct ", "const ", "volatile "):
        if n.startswith(p):
            n = n[len(p):]
    return n


def _is_info_type(t: lldb.SBType) -> bool:
    """Identify the 'mrdocs::Symbol' base to show it first."""
    return _norm_typename(t) == "mrdocs::Symbol"


class SymbolLikeSyntheticProvider:
    """
    Show fields from mrdocs::Symbol FIRST, then other bases (deepest→most-derived), then own fields.
    Does not recurse into sub-fields.
    """

    def __init__(self, valobj, _dict):
        self.root = _deref_if_ptr_or_ref(valobj)
        self.children = []
        self.update()

    def _append_own_fields(self, v: lldb.SBValue, seen: set[str], rank: int, decl: str):
        """Append this node's data members with stable, prefixed display names."""
        t = v.GetType()
        if not t or not t.IsValid():
            return
        for tm in _get_type_fields(t):
            try:
                if tm.IsBaseClass():
                    continue
            except Exception:
                pass
            fname = tm.GetName()
            if not fname:
                continue
            child = v.GetChildMemberWithName(fname)
            if not child or not child.IsValid():
                continue

            parts = []
            if _PREFIX_ORDER:
                parts.append(f"{rank:02d}")
            parts.append(f"{decl}::{fname}" if (_INCLUDE_DECL_TYPE and decl) else fname)
            name = " ".join(parts)

            base = name
            k = 2
            while name in seen:
                name = f"{base} #{k}"
                k += 1
            seen.add(name)

            try:
                child.SetName(name)
            except Exception:
                pass
            self.children.append(child)

    def _linearize_bases_postorder(self, v: lldb.SBValue):
        """Return nodes in post-order: [deepest bases …, most-derived (v)]."""
        order = []

        def dfs(node: lldb.SBValue):
            t = node.GetType()
            if not t or not t.IsValid():
                return
            for btm in _get_direct_bases(t):
                base_v = _base_value_from_typemember(node, btm)
                if base_v and base_v.IsValid():
                    dfs(base_v)
            order.append(node)

        dfs(v)
        return order

    def update(self):
        """Rebuild flat child list: Symbol base(s) first, then others, then self."""
        self.children = []
        v = self.root
        if not v or not v.IsValid():
            return

        seen = set()
        order = self._linearize_bases_postorder(v)
        if order:
            info_nodes = [n for n in order if _is_info_type(n.GetType())]
            other_nodes = [n for n in order if not _is_info_type(n.GetType())]
            rank = 0
            for node in info_nodes + other_nodes:
                self._append_own_fields(node, seen, rank, _norm_typename(node.GetType()))
                rank += 1

        if not self.children:
            n = v.GetNumChildren()
            for i in range(n):
                c = v.GetChildAtIndex(i)
                if c and c.IsValid():
                    self.children.append(c)

    # required API
    def has_children(self): return len(self.children) > 0
    def num_children(self): return len(self.children)
    def get_child_at_index(self, idx): return self.children[idx] if 0 <= idx < len(self.children) else None
    def get_child_index(self, name):
        for i, c in enumerate(self.children):
            if c.GetName() == name:
                return i
        return -1


# =============================================================================
# Optional<T>: summary + synthetic (handles libc++/libstdc++)
# =============================================================================

def _is_std_optional_typename(tyname: str | None) -> bool:
    """Heuristic: does the name look like a std::optional specialization (any vendor)."""
    if not tyname:
        return False
    tn = tyname.replace("class ", "").replace("struct ", "").replace("const ", "").replace("volatile ", "")
    return ("optional<" in tn and "std::" in tn) or "__optional" in tn or "::optional<" in tn


# Known payload/flag spellings across libstdc++/libc++
_OPT_PAYLOAD_NAMES = ("__val__", "__val_", "_M_value", "_M_payload", "__value_")
_OPT_ENGAGED_NAMES = ("__engaged_", "_M_engaged", "_M_has_value", "__has_value_")

def _nonsynth(vo):
    """Prefer non-synthetic view (raw layout) when available."""
    try:
        ns = vo.GetNonSyntheticValue()
        return ns if (ns and ns.IsValid()) else vo
    except Exception:
        return vo

def _value_from_typemember(parent_v, tm):
    """Materialize a field from its SBTypeMember (offset-aware)."""
    ft = tm.GetType()
    if not (ft and ft.IsValid()):
        return None
    try:
        off = tm.GetOffsetInBytes()
    except Exception:
        off_bits = tm.GetOffsetInBits() if hasattr(tm, "GetOffsetInBits") else 0
        off = off_bits // 8
    ch = parent_v.CreateChildAtOffset(tm.GetName() or "", off, ft)
    return ch if (ch and ch.IsValid()) else None

def _find_optional_payload(s):
    """
    From std::optional storage 's' (non-synthetic), return (payload_vo, engaged_bool_or_None).
    Works across libc++/libstdc++ raw layouts by scanning fields/bases.
    """
    s = _nonsynth(s)
    if not (s and s.IsValid()):
        return (None, None)

    t = s.GetType()
    if t and t.IsValid():
        engaged = None
        # own fields
        for tm in _get_type_fields(t):
            nm = tm.GetName() or ""
            if nm in _OPT_ENGAGED_NAMES:
                flag = _value_from_typemember(s, tm)
                if flag and flag.IsValid():
                    try:
                        engaged = flag.GetValueAsUnsigned(0) != 0
                    except Exception:
                        engaged = None
            if nm in _OPT_PAYLOAD_NAMES:
                val = _value_from_typemember(s, tm)
                if val and val.IsValid():
                    return (val, engaged)
        # bases
        for btm in _get_direct_bases(t):
            base_v = _base_value_from_typemember(s, btm)
            if base_v and base_v.IsValid():
                val, eng = _find_optional_payload(base_v)
                if val:
                    return (val, engaged if engaged is not None else eng)

    # last resort: direct child lookup by name
    for nm in _OPT_PAYLOAD_NAMES:
        ch = s.GetChildMemberWithName(nm)
        if ch and ch.IsValid():
            return (ch, None)

    return (None, None)

def _resolve_inner(valobj):
    """
    Resolve the contained value of mrdocs::Optional<T>.
    Returns:
      - SBValue of T (or T* for Optional<T*>), or
      - None if disengaged/unavailable.
    """
    vo = _deref_if_ptr_or_ref(valobj)
    if not (vo and vo.IsValid()):
        return None

    # Optional<T&> path: p_ is a T*
    p = _get_field_value_by_offset(vo, "p_") or vo.GetChildMemberWithName("p_")
    if p is not None and p.IsValid():
        try:
            if p.GetValueAsUnsigned(0) == 0:
                return None
        except Exception:
            pv = p.GetValue()
            if not pv or pv in ("0", "(nullptr)", "NULL"):
                return None
        deref = p.Dereference()
        return deref if (deref and deref.IsValid()) else None

    # Optional<T> path: s_ is inline T or std::optional<T>
    s = _get_field_value_by_offset(vo, "s_") or vo.GetChildMemberWithName("s_")
    if not (s and s.IsValid()):
        return None

    # A) Friendly synthetic with a 'Value' child
    try:
        n = s.GetNumChildren()
    except Exception:
        n = 0
    if n > 0:
        for i in range(n):
            ch = s.GetChildAtIndex(i)
            if ch and ch.IsValid() and (ch.GetName() or "") == "Value":
                return ch
        # don't return s here: synthetic may be libc++ internals; fall through

    # B) Raw layout: find payload (__val_ / _M_value) and engaged flag
    payload, engaged = _find_optional_payload(s)
    if engaged is False:
        return None
    if payload and payload.IsValid():
        return payload

    # C) Nullable-traits inline T: s_ is the value
    return s


def OptionalSummaryProvider(valobj, _dict):
    """
    Summary for mrdocs::Optional<T>.
    - Disengaged: 'nullopt'
    - T*       : '0xaddr[ → <short pointee>]'
    - Scalars  : value
    - Strings  : string (no quotes)
    - Structs  : '' (empty summary; children show content)
    """
    try:
        inner = _resolve_inner(valobj)
        if inner is None:
            return "nullopt"

        # T* : compact address; optional hint with short pointee summary/value
        t = inner.GetType()
        if t and t.IsValid() and t.IsPointerType():
            addr = _fmt_ptr_addr(inner)
            hint = ""
            try:
                dv = inner.Dereference()
                if dv and dv.IsValid():
                    hs = _clean(dv.GetSummary()) or _clean(dv.GetValue())
                    if hs:
                        hint = f" → {hs}"
            except Exception:
                pass
            return f"{addr}{hint}"

        # Scalars/strings
        s = _clean(inner.GetSummary())
        if s not in (None, ""):
            return s
        v = _clean(inner.GetValue())
        if v not in (None, ""):
            return v

        # Structured types: quiet summary; show children
        try:
            if inner.GetNumChildren() > 0 or inner.MightHaveChildren():
                return ""
        except Exception:
            pass

        d = _clean(inner.GetObjectDescription())
        if d not in (None, ""):
            return d
        return ""
    except Exception as e:
        return f"<summary error: {e}>"


class OptionalTransparentSyntheticProvider:
    """
    Synthetic for mrdocs::Optional<T> that forwards to the contained value.
    - For T* the children are those of *T (summary still shows the pointer).
    """

    def __init__(self, valobj, _dict):
        self.valobj = valobj
        self.inner = None          # SBValue of T or T*
        self.children_root = None  # SBValue used to enumerate children (T or *T)
        self.update()

    def update(self):
        """Recompute forward target and children root (dereference T* if needed)."""
        self.inner = _resolve_inner(self.valobj)
        self.children_root = self.inner
        if self.inner and self.inner.IsValid():
            t = self.inner.GetType()
            if t and t.IsValid() and t.IsPointerType():
                try:
                    deref = self.inner.Dereference()
                    if deref and deref.IsValid():
                        self.children_root = deref
                except Exception:
                    pass

    def has_children(self):
        r = self.children_root
        return bool(r) and (r.GetNumChildren() > 0 or r.MightHaveChildren())

    def num_children(self):
        r = self.children_root
        return r.GetNumChildren() if r else 0

    def get_child_at_index(self, i):
        r = self.children_root
        if not r:
            return None
        return r.GetChildAtIndex(i)

    def get_child_index(self, name):
        r = self.children_root
        if not r:
            return -1
        n = r.GetNumChildren()
        for idx in range(n):
            c = r.GetChildAtIndex(idx)
            if c and c.IsValid() and c.GetName() == name:
                return idx
        return -1


# =============================================================================
# Location: summary only ("ShortPath:LineNumber [doc]")
# =============================================================================

def LocationSummaryProvider(valobj, _dict):
    """Summary for mrdocs::Location → 'ShortPath:LineNumber[ [doc]]'."""
    try:
        v = _deref_if_ptr_or_ref(valobj)
        if not v or not v.IsValid():
            return "<unavailable object>"

        sp = _get_field_value_by_offset(v, "ShortPath") or v.GetChildMemberWithName("ShortPath")
        ln = _get_field_value_by_offset(v, "LineNumber") or v.GetChildMemberWithName("LineNumber")
        dn = _get_field_value_by_offset(v, "Documented") or v.GetChildMemberWithName("Documented")

        path = _str_from_std_string(sp) if sp and sp.IsValid() else None
        if not path:
            return "<unknown-path>"

        line = ln.GetValueAsUnsigned(0) if (ln and ln.IsValid()) else 0
        documented = (dn.GetValueAsUnsigned(0) != 0) if (dn and dn.IsValid()) else False
        return f"{path}:{line}{' [doc]' if documented else ''}"
    except Exception as e:
        return f"<summary error: {e}>"


# =============================================================================
# Polymorphic<T>: summary + synthetic
# =============================================================================

def _poly_inner_value(valobj):
    """
    Resolve Polymorphic<T>::WB (WrapperBase*).
    - nullptr → "null"
    - else deref, take dynamic type, return its 'Value' field (own or in bases).
    """
    v = _deref_if_ptr_or_ref(valobj)
    if not v or not v.IsValid():
        return None

    wb = (_child_by_name_any(v, ("WB", "wb", "WB_", "wb_"))
          or _get_field_value_by_offset(v, "WB"))
    if not wb or not wb.IsValid():
        return None
    if wb.GetValueAsUnsigned(0) == 0:
        return "null"

    pointee = wb.Dereference()
    if not pointee or not pointee.IsValid():
        return None
    dyn = pointee.GetDynamicValue(lldb.eDynamicCanRunTarget)
    wrapper = dyn if dyn and dyn.IsValid() else pointee
    try:
        wrapper = wrapper.GetNonSyntheticValue() or wrapper
    except Exception:
        pass

    value = (_child_by_name_any(wrapper, ("Value", "value", "Value_", "value_"))
             or _find_field_recursive_any(wrapper, ("Value", "value", "Value_", "value_")))
    return value if value and value.IsValid() else None


def PolymorphicSummaryProvider(valobj, _dict):
    """Summary for mrdocs::Polymorphic<T>: forward T’s summary/value; null → std::nullopt."""
    try:
        inner = _poly_inner_value(valobj)
        if inner == "null":
            return "std::nullopt"
        if not inner:
            return "<unavailable>"

        s = _clean(inner.GetSummary())
        if s is not None:
            return s
        v = _clean(inner.GetValue())
        if v is not None:
            return v
        d = _clean(inner.GetObjectDescription())
        if d is not None:
            return d
        return "<unavailable>"
    except Exception as e:
        return f"<summary error: {e}>"


class PolymorphicTransparentSyntheticProvider:
    """Synthetic for mrdocs::Polymorphic<T>: expose inner 'Value' as children."""
    def __init__(self, valobj, _dict):
        self.valobj = valobj
        self.inner = None
        self.update()

    def update(self):
        iv = _poly_inner_value(self.valobj)
        self.inner = None if (iv is None or iv == "null") else iv
        self.children_root = self.inner

        if self.inner and self.inner.IsValid():
            try:
                self.inner.SetPreferDynamicValue(lldb.eDynamicCanRunTarget)
                self.inner.SetPreferSyntheticValue(True)
            except Exception:
                pass

            t = self.inner.GetType()
            if t and t.IsValid() and t.IsPointerType():
                try:
                    deref = self.inner.Dereference()
                    if deref and deref.IsValid():
                        self.children_root = deref
                        # also ensure prefs on the deref we’ll enumerate
                        self.children_root.SetPreferDynamicValue(lldb.eDynamicCanRunTarget)
                        self.children_root.SetPreferSyntheticValue(True)
                except Exception:
                    pass

    def has_children(self):
        return bool(self.inner) and (self.inner.GetNumChildren() > 0 or self.inner.MightHaveChildren())

    def num_children(self):
        return self.inner.GetNumChildren() if self.inner else 0

    def get_child_at_index(self, i):
        if not self.inner:
            return None
        return self.inner.GetChildAtIndex(i)

    def get_child_index(self, name):
        if not self.inner:
            return -1
        n = self.inner.GetNumChildren()
        for idx in range(n):
            c = self.inner.GetChildAtIndex(idx)
            if c and c.IsValid() and c.GetName() == name:
                return idx
        return -1


# =============================================================================
# Registration helpers & setup (uniform, zero hard-coded variant lists)
# =============================================================================

def _rx_variants(core: str) -> list[str]:
    """
    Build regex variants for a given C++ type name/pattern.

    Args:
      core:   Base name or pattern (without ^$). Examples:
              - 'mrdocs::Name'
              - 'mrdocs::Optional<.*>'
              - 'mrdocs::[^:]*Symbol'  (already a regex)
      allow_kind_prefixes:
              If True, generate 'class ' and 'struct ' prefixed variants.
              If False, don't (useful for templated regexes).
      allow_const:
              If True, generate 'const ' prefixed variants.

    Returns:
      List of '^...$' anchored regex strings.
    """
    kinds = ["", "class ", "struct "]
    consts = ["", "const "]
    out = []
    for c in consts:
        for k in kinds:
            # Note: const must go before class/struct, e.g. 'const class T'
            prefix = f"{c}{k}".strip()
            space = (prefix + " ") if prefix else ""
            out.append(f"^{space}{core}$")
    return out

# -------- Name summary: "Prefix::Name" or "Name" --------

# --- helpers for Name prefix chain (names only, no ids) ---

def _nameinfo_path_from(ni: lldb.SBValue) -> str | None:
    """Return 'A::B::C' built from Name::Name along the Prefix chain. No ids."""
    if not ni or not ni.IsValid():
        return None
    # current name
    nm_v = _get_field_value_by_offset(ni, "Name") or ni.GetChildMemberWithName("Name")
    nm = _str_from_std_string(nm_v) if (nm_v and nm_v.IsValid()) else None
    if not nm:
        nm = "<no Name>"

    # recurse into Prefix (Polymorphic<Name>)
    pref_poly = _get_field_value_by_offset(ni, "Prefix") or ni.GetChildMemberWithName("Prefix")
    if pref_poly and pref_poly.IsValid():
        inner = _poly_inner_value(pref_poly)  # "null" | SBValue(Name) | None
        if inner and inner != "null":
            parent = _nameinfo_path_from(inner)
            return f"{parent}::{nm}" if parent else nm
    return nm


def _hex_from_symbolid(sbv) -> str | None:
    """Return 40-char hex for SymbolID if readable; '' if invalid (all zeros); None on failure."""
    bs = _extract_symbolid_bytes(sbv, 20)
    if bs is None:
        return None
    if all(b == 0x00 for b in bs):
        return ''
    return ''.join(f'{b:02x}' for b in bs)


# --- Name summary: "Prefix::Name (idhex)" or "Name" ---

def NameSummaryProvider(valobj, _dict):
    try:
        v = _deref_if_ptr_or_ref(valobj)
        if not v or not v.IsValid():
            return "<unavailable>"

        # Name (this object)
        name_v = _get_field_value_by_offset(v, "Name") or v.GetChildMemberWithName("Name")
        name = _str_from_std_string(name_v) if (name_v and name_v.IsValid()) else None
        if not name:
            name = "<no Name>"

        # Prefix chain (names only)
        prefix_str = None
        pref_poly = _get_field_value_by_offset(v, "Prefix") or v.GetChildMemberWithName("Prefix")
        if pref_poly and pref_poly.IsValid():
            inner = _poly_inner_value(pref_poly)
            if inner and inner != "null":
                prefix_str = _nameinfo_path_from(inner)  # no ids

        # This object's SymbolID (append only if non-zero)
        sid_hex = None
        sid_v = _get_field_value_by_offset(v, "id") or v.GetChildMemberWithName("id")
        if sid_v and sid_v.IsValid():
            sid_hex = _hex_from_symbolid(sid_v)  # None=unreadable, ''=all zeros

        # Assemble: prefix::name (id)
        core = f"{prefix_str}::{name}" if prefix_str else name
        if sid_hex:  # non-empty means non-zero id
            core += f" ({sid_hex})"
        return core
    except Exception as e:
        return f"<summary error: {e}>"

class FlattenAllBasesSyntheticProvider:
    """
    Flatten data members from all base classes (deepest → most-derived), then own members.
    Does not recurse into sub-fields. Skips base-class nodes as direct children.
    Drop-in generic provider for “show me everything flat”.
    """

    # knobs (override per-class by assigning on the instance in __init__ if you want)
    EMIT_SELF_LAST = True         # after all bases, append the most-derived's own fields
    INCLUDE_DECL_TYPE = _INCLUDE_DECL_TYPE
    PREFIX_ORDER = _PREFIX_ORDER

    def __init__(self, valobj, _dict):
        self.root = _deref_if_ptr_or_ref(valobj)
        self.children = []
        self._err = None
        self.update()

    # ----- helpers -----

    def _norm_typename(self, t: lldb.SBType) -> str:
        n = _type_name(t)
        for p in ("class ", "struct ", "const ", "volatile "):
            if n.startswith(p):
                n = n[len(p):]
        return n

    def _append_own_fields(self, v: lldb.SBValue, seen: set[str], rank: int, decl: str):
        t = v.GetType()
        if not t or not t.IsValid():
            return
        for tm in _get_type_fields(t):
            # only data members, never a base node
            try:
                if tm.IsBaseClass():
                    continue
            except Exception:
                # older LLDBs may not have IsBaseClass; rely on data-path fallback
                pass

            fname = tm.GetName()
            if not fname:
                continue

            child = v.GetChildMemberWithName(fname)
            if not child or not child.IsValid():
                continue

            # build display name
            parts = []
            if self.PREFIX_ORDER:
                parts.append(f"{rank:02d}")
            parts.append(f"{decl}::{fname}" if (self.INCLUDE_DECL_TYPE and decl) else fname)
            name = " ".join(parts)

            # de-dup same display labels
            base = name
            k = 2
            while name in seen:
                name = f"{base} #{k}"
                k += 1
            seen.add(name)

            try:
                child.SetName(name)
            except Exception:
                pass

            self.children.append(child)

    def _linearize_bases_postorder(self, v: lldb.SBValue):
        """Nodes in post-order: [deepest bases …, most-derived (v)]."""
        order = []
        visited_types = set()  # avoid re-visiting virtual bases / diamonds

        def dfs(node: lldb.SBValue):
            t = node.GetType()
            if not t or not t.IsValid():
                return
            # Use type name as a proxy key; SBType equality can be flaky across modules
            key = self._norm_typename(t)
            if key in visited_types:
                return
            visited_types.add(key)

            for btm in _get_direct_bases(t):
                base_v = _base_value_from_typemember(node, btm)
                if base_v and base_v.IsValid():
                    dfs(base_v)

            order.append(node)

        dfs(v)
        return order

    # ----- required API -----

    def update(self):
        self.children = []
        v = self.root
        if not v or not v.IsValid():
            return

        try:
            order = self._linearize_bases_postorder(v)
            if not order:
                # nothing to flatten → fall back
                n = v.GetNumChildren()
                for i in range(n):
                    c = v.GetChildAtIndex(i)
                    if c and c.IsValid():
                        self.children.append(c)
                return

            # Optionally place self (most-derived) last
            if self.EMIT_SELF_LAST and order[-1] is v:
                bases = order[:-1]
                tail = [order[-1]]
            else:
                bases = order
                tail = []

            seen = set()
            rank = 0
            for node in bases:
                self._append_own_fields(node, seen, rank, self._norm_typename(node.GetType()))
                rank += 1
            for node in tail:
                self._append_own_fields(node, seen, rank, self._norm_typename(node.GetType()))
                rank += 1

            # final fallback if nothing was emitted
            if not self.children:
                n = v.GetNumChildren()
                for i in range(n):
                    c = v.GetChildAtIndex(i)
                    if c and c.IsValid():
                        self.children.append(c)

        except Exception as e:
            # defensive: on any unexpected LLDB quirk, degrade gracefully
            self._err = str(e)
            n = v.GetNumChildren()
            for i in range(n):
                c = v.GetChildAtIndex(i)
                if c and c.IsValid():
                    self.children.append(c)

    def has_children(self): return len(self.children) > 0
    def num_children(self): return len(self.children)
    def get_child_at_index(self, idx): return self.children[idx] if 0 <= idx < len(self.children) else None
    def get_child_index(self, name):
        for i, c in enumerate(self.children):
            try:
                if c.GetName() == name:
                    return i
            except Exception:
                pass
        return -1

def _escape_for_summary(s: str) -> str:
    # C-ish minimal escaping for debugger summaries
    s = s.replace("\\", "\\\\").replace("\"", "\\\"")
    s = s.replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t")
    return s

def _kind_string(v: lldb.SBValue) -> str:
    k = _get_field_value_by_offset(v, "Kind") or v.GetChildMemberWithName("Kind")
    if not (k and k.IsValid()):
        return "Inline"
    return (_clean(k.GetSummary()) or _clean(k.GetValue()) or "Inline")

def _inline_text(vo: lldb.SBValue, depth: int = 0, max_depth: int = 16, max_children: int = 512) -> str:
    """
    Extract *textual* content of an Inline:
      - literal → its contents
      - children → concatenation of children's textual contents (recursively)
      - otherwise → ""
    (Does NOT prepend Kind; meant for building the quoted part.)
    """
    if not (vo and vo.IsValid()):
        return ""

    v = _deref_if_ptr_or_ref(vo)

    # literal
    lit = _get_field_value_by_offset(v, "literal") or v.GetChildMemberWithName("literal")
    if lit and lit.IsValid():
        s = _str_from_std_string(lit)
        return s or ""

    # children
    ch = _get_field_value_by_offset(v, "children") or v.GetChildMemberWithName("children")
    if ch and ch.IsValid():
        try:
            ch.SetPreferSyntheticValue(True)
        except Exception:
            pass
        n = ch.GetNumChildren()
        if n <= 0 or depth >= max_depth:
            return ""
        pieces = []
        limit = min(n, max_children)
        for i in range(limit):
            el = ch.GetChildAtIndex(i)
            if not (el and el.IsValid()):
                continue
            inner = _poly_inner_value(el)
            if inner == "null":
                continue
            inner = inner or el
            try:
                inner.SetPreferSyntheticValue(True)
                inner.SetPreferDynamicValue(lldb.eDynamicCanRunTarget)
            except Exception:
                pass
            pieces.append(_inline_text(inner, depth + 1, max_depth, max_children))
        return "".join(pieces)

    # nothing textual here
    return ""

def InlineSummaryProvider(valobj, _dict):
    """
    Summary for mrdocs::doc::<...>Inline:
      1) Kind: "escaped text"    (literal or children concatenation)
      2) Kind                     (no literal/children)
    """
    try:
        v = _deref_if_ptr_or_ref(valobj)
        if not (v and v.IsValid()):
            return ""

        kind = _kind_string(v)

        # textual content (literal or concatenated children)
        text = _inline_text(v)
        if text:
            return f'{kind}: "{_escape_for_summary(text)}"'

        # no textual content → just Kind
        return kind
    except Exception as e:
        return f"<summary error: {e}>"

def __lldb_init_module(debugger, _dict):
    """
    Register all summaries/synthetics in category 'MrDocs' using uniform variant generation.
    """
    cat = "MrDocs"

    # Symbol-like (regex pattern + const variant; no class/struct added)
    for rx in _rx_variants("mrdocs::[^:]*Symbol"):
        debugger.HandleCommand(f'type summary   add -w {cat} -x -P -F mrdocs_formatters.SymbolLikeSummaryProvider "{rx}"')
        debugger.HandleCommand(f'type synthetic add -w {cat} -x -l mrdocs_formatters.SymbolLikeSyntheticProvider "{rx}"')

    # SymbolID (concrete type; include class/struct & const variants)
    for rx in _rx_variants("mrdocs::SymbolID"):
        debugger.HandleCommand(f'type summary add -w {cat} -x -P -F mrdocs_formatters.SymbolIDSummaryProvider "{rx}"')

    # Location (concrete type with variants)
    for rx in _rx_variants("mrdocs::Location"):
        debugger.HandleCommand(f'type summary add -w {cat} -x -P -F mrdocs_formatters.LocationSummaryProvider "{rx}"')

    # Name (concrete type; include class/struct & const variants)
    for rx in _rx_variants("mrdocs::Name"):
        debugger.HandleCommand(f'type summary add -w {cat} -x -P -F mrdocs_formatters.NameSummaryProvider "{rx}"')

    # Documentation: use the flattener
    for rx in _rx_variants("mrdocs::doc::[^:]*Block") + _rx_variants("mrdocs::doc::[^:]*Inline"):
        debugger.HandleCommand(f'type synthetic add -w {cat} -x -l mrdocs_formatters.FlattenAllBasesSyntheticProvider "{rx}"')

    # Optional<T> (templated regex; don't add class/struct; do add const)
    for rx in _rx_variants("mrdocs::Optional<.*>"):
        debugger.HandleCommand(f'type summary   add -w {cat} -x -P -F mrdocs_formatters.OptionalSummaryProvider "{rx}"')
        debugger.HandleCommand(f'type synthetic add -w {cat} -x -l mrdocs_formatters.OptionalTransparentSyntheticProvider "{rx}"')

    # Polymorphic<T> (templated regex; no class/struct; add const)
    for rx in _rx_variants("mrdocs::Polymorphic<.*>"):
        debugger.HandleCommand(f'type summary   add -w {cat} -x -P -F mrdocs_formatters.PolymorphicSummaryProvider "{rx}"')
        debugger.HandleCommand(f'type synthetic add -w {cat} -x -l mrdocs_formatters.PolymorphicTransparentSyntheticProvider "{rx}"')

    for rx in _rx_variants("mrdocs::doc::[^:]*Inline"):
        debugger.HandleCommand(f'type summary add -w {cat} -x -P -F mrdocs_formatters.InlineSummaryProvider "{rx}"')

    debugger.HandleCommand(f"type category enable {cat}")
