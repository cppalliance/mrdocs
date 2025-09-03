#
# Copyright (c) 2025 alandefreitas (alandefreitas@gmail.com)
#
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt
#

import lldb

# ---------- SIMPLE, WORKING REGEXES ----------
# Tip: register BOTH const and non-const; avoid fancy groups.
_INFO_REGEXES = [
    r"^clang::mrdocs::.*Info$",
    r"^const clang::mrdocs::.*Info$",
]


# ---------- POINTER/REF SAFE ----------
def _deref_if_ptr_or_ref(v: lldb.SBValue) -> lldb.SBValue:
    t = v.GetType()
    if not t or not t.IsValid():
        return v
    if t.IsPointerType() or t.IsReferenceType():
        dv = v.Dereference()
        return dv if dv and dv.IsValid() else v
    return v


# ---------- TYPE INTROSPECTION HELPERS ----------
def _type_name(t: lldb.SBType) -> str:
    return (t.GetName() or "") if (t and t.IsValid()) else ""


def _get_type_fields(t: lldb.SBType):
    try:
        n = t.GetNumberOfFields()
    except Exception:
        n = 0
    for i in range(n):
        yield t.GetFieldAtIndex(i)  # SBTypeMember


def _get_direct_bases(t: lldb.SBType):
    try:
        n = t.GetNumberOfDirectBaseClasses()
    except Exception:
        n = 0
    for i in range(n):
        yield t.GetDirectBaseClassAtIndex(i)  # SBTypeMember


def _base_value_from_typemember(parent_v: lldb.SBValue, base_tm: lldb.SBTypeMember) -> lldb.SBValue | None:
    """
    Robustly get the base subobject SBValue from the parent, using the base type and byte offset.
    """
    bt = base_tm.GetType()
    if not bt or not bt.IsValid():
        return None
    # Offset is in bits in some LLDBs; prefer GetOffsetInBytes() if available.
    try:
        off = base_tm.GetOffsetInBytes()
    except Exception:
        off_bits = base_tm.GetOffsetInBits() if hasattr(base_tm, "GetOffsetInBits") else 0
        off = off_bits // 8
    child = parent_v.CreateChildAtOffset(_type_name(bt) or "__base", off, bt)
    return child if child and child.IsValid() else None


# ---------- STRING EXTRACTION (std::string only, per your note) ----------
def _string_from_std_string(sbv: lldb.SBValue) -> str | None:
    if not sbv or not sbv.IsValid():
        return None
    # LLDB usually gives a quoted summary for std::string
    s = sbv.GetSummary() or sbv.GetObjectDescription() or sbv.GetValue()
    if not s:
        return None
    s = s.strip()
    if len(s) >= 2 and s[0] == '"' and s[-1] == '"':
        s = s[1:-1]
    return s


# ---------- FIND Name IN SELF OR BASES (std::string only) ----------
_CANDIDATES = ("Name", "name", "Name_", "name_")  # add others if your field is different


def _find_std_string_name(v: lldb.SBValue) -> lldb.SBValue | None:
    v = _deref_if_ptr_or_ref(v)
    if not v or not v.IsValid():
        return None
    t = v.GetType()
    if not t or not t.IsValid():
        return None

    # 1) own data members
    for tm in _get_type_fields(t):
        try:
            if tm.IsBaseClass():
                continue
        except Exception:
            pass
        fname = tm.GetName() or ""
        if fname not in _CANDIDATES:
            continue
        child = v.GetChildMemberWithName(fname)
        if child and child.IsValid():
            # rely on LLDB’s std::string summary (no need to check its exact SBType)
            if _string_from_std_string(child) is not None:
                return child

    # 2) direct bases (RELIABLE path using offset)
    for btm in _get_direct_bases(t):
        base_v = _base_value_from_typemember(v, btm)
        if base_v:
            got = _find_std_string_name(base_v)  # recurse
            if got:
                return got
    return None


# ---------- SUMMARY ----------
def InfoLikeSummaryProvider(valobj, _dict):
    try:
        name_val = _find_std_string_name(valobj)
        if not name_val:
            return "<no Name>"
        s = _string_from_std_string(name_val)
        return s if s else "<Name unreadable>"
    except Exception as e:
        return f"<summary error: {e}>"


# -------- SymbolID summary --------

# Optional: set to True if you prefer uppercase hex
_SYMBOLID_HEX_UPPER = False


def _get_field_value_by_offset(parent_v: lldb.SBValue, field_name: str) -> lldb.SBValue | None:
    """Create the SBValue for a named field using its exact offset+type (robust across toolchains)."""
    t = parent_v.GetType()
    if not t or not t.IsValid():
        return None
    # First try: exact match via SBTypeMember
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
            if not ftype or not ftype.IsValid():
                break
            try:
                off = tm.GetOffsetInBytes()
            except Exception:
                off_bits = tm.GetOffsetInBits() if hasattr(tm, "GetOffsetInBits") else 0
                off = off_bits // 8
            v = parent_v.CreateChildAtOffset(field_name, off, ftype)
            return v if v and v.IsValid() else None
    # Fallback: if toolchain exposes it directly
    v = parent_v.GetChildMemberWithName(field_name)
    return v if v and v.IsValid() else None


def _read_process_bytes(valobj: lldb.SBValue, addr: int, n: int) -> bytes | None:
    if addr == 0 or n <= 0:
        return None
    proc = valobj.GetProcess()
    if not proc or not proc.IsValid():
        return None
    err = lldb.SBError()
    data = proc.ReadMemory(addr, n, err)
    return data if err.Success() and data is not None and len(data) == n else None


def _extract_symbolid_bytes(sym: lldb.SBValue, expected_len: int = 20) -> list[int] | None:
    sym = _deref_if_ptr_or_ref(sym)
    if not sym or not sym.IsValid():
        return None
    data_field = _get_field_value_by_offset(sym, "data_")
    if not data_field or not data_field.IsValid():
        return None

    # Route A: child iteration (if LLDB exposes array elements)
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

    # Route B: direct memory read from the array address
    addr_v = data_field.AddressOf()
    if addr_v and addr_v.IsValid():
        addr = addr_v.GetValueAsUnsigned()
        raw = _read_process_bytes(sym, addr, expected_len)
        if raw is not None:
            return [b for b in raw]

    return None


def SymbolIDSummaryProvider(valobj, _dict):
    try:
        bs = _extract_symbolid_bytes(valobj, 20)
        if not bs:
            return "<unavailable>"
        if all(b == 0xFF for b in bs):
            return "<global>"
        if all(b == 0x00 for b in bs):
            return "std::nullopt"
        if _SYMBOLID_HEX_UPPER:
            return "".join(f"{b:02X}" for b in bs)
        else:
            return "".join(f"{b:02x}" for b in bs)
    except Exception as e:
        return f"<summary error: {e}>"


# ---------- SYNTHETIC: FLATTEN BASES ONLY ----------
# --- knobs (same as before) ---
_PREFIX_ORDER = True  # add "NN " so alphabetical sort preserves order
_INCLUDE_DECL_TYPE = True  # include "DeclType::" before the field name


def _norm_typename(t: lldb.SBType) -> str:
    n = _type_name(t)
    # strip common prefixes/qualifiers
    for p in ("class ", "struct ", "const ", "volatile "):
        if n.startswith(p):
            n = n[len(p):]
    return n


def _is_info_type(t: lldb.SBType) -> bool:
    return _norm_typename(t) == "clang::mrdocs::Info"


class InfoLikeSyntheticProvider:
    """
    Show fields from clang::mrdocs::Info FIRST,
    then all other bases deepest-first, then most-derived.
    Never recurses into fields. Uses order prefixes to beat UI resorting.
    """

    def __init__(self, valobj, _dict):
        self.root = _deref_if_ptr_or_ref(valobj)
        self.children = []
        self.update()

    def _append_own_fields(self, v: lldb.SBValue, seen: set[str], rank: int, decl: str):
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

            # Build display name with stable, sortable prefix
            parts = []
            if _PREFIX_ORDER:
                parts.append(f"{rank:02d}")
            if _INCLUDE_DECL_TYPE and decl:
                parts.append(f"{decl}::{fname}")
            else:
                parts.append(fname)
            name = " ".join(parts)

            # Dedup if collisions happen
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
        """
        Returns nodes in post-order: [deepest bases ..., most-derived (v)].
        """
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
        self.children = []
        v = self.root
        if not v or not v.IsValid():
            return

        seen = set()
        order = self._linearize_bases_postorder(v)  # deepest … -> most-derived
        if order:
            # Partition: Info nodes first, others after (preserving their relative order)
            info_nodes = [n for n in order if _is_info_type(n.GetType())]
            other_nodes = [n for n in order if not _is_info_type(n.GetType())]

            rank = 0
            for node in info_nodes:
                self._append_own_fields(node, seen, rank, _norm_typename(node.GetType()))
                rank += 1
            for node in other_nodes:
                self._append_own_fields(node, seen, rank, _norm_typename(node.GetType()))
                rank += 1

        # Fallback: nothing discovered → show immediate children (no field recursion)
        if not self.children:
            n = v.GetNumChildren()
            for i in range(n):
                c = v.GetChildAtIndex(i)
                if c and c.IsValid():
                    self.children.append(c)

    # required API
    def has_children(self):
        return len(self.children) > 0

    def num_children(self):
        return len(self.children)

    def get_child_at_index(self, idx):
        return self.children[idx] if 0 <= idx < len(self.children) else None

    def get_child_index(self, name):
        for i, c in enumerate(self.children):
            if c.GetName() == name:
                return i
        return -1


# -------- Optional<T>: transparent summary + children (fixed) --------

def _optional_inner(valobj):
    v = _deref_if_ptr_or_ref(valobj)
    if not v or not v.IsValid():
        return None
    inner = _get_field_value_by_offset(v, "t_") or v.GetChildMemberWithName("t_")
    return inner if inner and inner.IsValid() else None


def _clean(s: str | None) -> str | None:
    if not s:
        return None
    s = s.strip()
    if s.startswith("error:"):
        return None
    return s


def OptionalSummaryProvider(valobj, _dict):
    try:
        inner = _optional_inner(valobj)
        if not inner:
            return "<unavailable>"

        # 1) Prefer T's own summary (works with your other formatters)
        s = _clean(inner.GetSummary())
        if s is not None:
            return s

        # 2) Safe fallbacks without type introspection
        v = _clean(inner.GetValue())
        if v is not None:
            return v

        d = _clean(inner.GetObjectDescription())
        if d is not None:
            return d

        return "<unavailable summary>"
    except Exception as e:
        return f"<summary error: {e}>"


class OptionalTransparentSyntheticProvider:
    """
    Present Optional<T> as if it were T: forward children to t_.
    """

    def __init__(self, valobj, _dict):
        self.valobj = valobj
        self.inner = None
        self.update()

    def update(self):
        self.inner = _optional_inner(self.valobj)

    def has_children(self):
        return bool(self.inner) and (self.inner.GetNumChildren() > 0 or self.inner.MightHaveChildren())

    def num_children(self):
        return self.inner.GetNumChildren() if self.inner else 0

    def get_child_at_index(self, i):
        if not self.inner: return None
        return self.inner.GetChildAtIndex(i)

    def get_child_index(self, name):
        if not self.inner: return -1
        n = self.inner.GetNumChildren()
        for idx in range(n):
            c = self.inner.GetChildAtIndex(idx)
            if c and c.IsValid() and c.GetName() == name:
                return idx
        return -1


# -------- Location summary: "ShortPath:LineNumber" --------

def _str_from_std_string(v: lldb.SBValue) -> str | None:
    if not v or not v.IsValid():
        return None
    s = v.GetSummary() or v.GetObjectDescription() or v.GetValue()
    if not s:
        return None
    s = s.strip()
    if len(s) >= 2 and s[0] == '"' and s[-1] == '"':
        s = s[1:-1]
    return s


def LocationSummaryProvider(valobj, _dict):
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

        line = 0
        if ln and ln.IsValid():
            line = ln.GetValueAsUnsigned(0)

        documented = False
        if dn and dn.IsValid():
            documented = dn.GetValueAsUnsigned(0) != 0

        return f"{path}:{line}{' [doc]' if documented else ''}"
    except Exception as e:
        return f"<summary error: {e}>"


# ---------- helpers for name-based child lookup ----------
def _strip_prefixes_for_match(nm: str) -> str:
    """Strip our NN prefixes and optional 'Decl::' we add in other providers."""
    if not nm:
        return nm
    # Remove leading "NN " prefix (e.g., "03 Info::Name")
    nm = re.sub(r"^\d{2}\s+", "", nm)
    # If a DeclType prefix exists, keep only the field after '::'
    if "::" in nm:
        nm = nm.split("::", 1)[1]
    # Also strip any trailing " #k" suffix used for de-duping
    nm = re.sub(r"\s+#\d+$", "", nm)
    return nm


def _child_by_name_any(v: lldb.SBValue, names: tuple[str, ...]) -> lldb.SBValue | None:
    """Try to get a child by name from both synthetic and non-synthetic views."""
    if not v or not v.IsValid():
        return None
    views = [v]
    try:
        ns = v.GetNonSyntheticValue()
        if ns and ns.IsValid() and ns.GetID() != v.GetID():
            views.append(ns)
    except Exception:
        pass

    # 1) Direct member lookup
    for view in views:
        for nm in names:
            ch = view.GetChildMemberWithName(nm)
            if ch and ch.IsValid():
                return ch

    # 2) Enumerate children and match by (possibly prefixed) name
    for view in views:
        n = view.GetNumChildren()
        for i in range(n):
            ch = view.GetChildAtIndex(i)
            if not ch or not ch.IsValid():
                continue
            nm = _strip_prefixes_for_match(ch.GetName() or "")
            if nm in names:
                return ch
    return None


def _find_field_recursive_any(v: lldb.SBValue, names: tuple[str, ...]) -> lldb.SBValue | None:
    """Search own fields for any name in `names`; recurse into direct bases (type+offset)."""
    if not v or not v.IsValid():
        return None
    # Try fast path on this node
    got = _child_by_name_any(v, names)
    if got and got.IsValid():
        return got

    # Try via SBType fields (offset-based)
    t = v.GetType()
    if t and t.IsValid():
        try:
            n = t.GetNumberOfFields()
        except Exception:
            n = 0
        for i in range(n):
            tm = t.GetFieldAtIndex(i)
            # skip bases
            try:
                if tm.IsBaseClass():
                    continue
            except Exception:
                pass
            fname = tm.GetName() or ""
            if fname in names:
                try:
                    off = tm.GetOffsetInBytes()
                except Exception:
                    off_bits = tm.GetOffsetInBits() if hasattr(tm, "GetOffsetInBits") else 0
                    off = off_bits // 8
                ft = tm.GetType()
                ch = v.CreateChildAtOffset(fname, off, ft) if ft and ft.IsValid() else None
                if ch and ch.IsValid():
                    return ch

        # Recurse into direct bases
        for btm in _get_direct_bases(t):
            base_v = _base_value_from_typemember(v, btm)
            if base_v and base_v.IsValid():
                got = _find_field_recursive_any(base_v, names)
                if got and got.IsValid():
                    return got
    return None


# ---------- Polymorphic<T>: forward to WB->Value ----------
def _poly_inner_value(valobj):
    """
    Resolve Polymorphic<T>::WB (WrapperBase*). If null -> "null".
    Else, deref to dynamic Wrapper<T> and return its 'Value' field.
    """
    v = _deref_if_ptr_or_ref(valobj)
    if not v or not v.IsValid():
        return None

    # Find WB robustly
    wb = (_child_by_name_any(v, ("WB", "wb", "WB_", "wb_"))
          or _get_field_value_by_offset(v, "WB"))
    if not wb or not wb.IsValid():
        return None

    # nullptr?
    if wb.GetValueAsUnsigned(0) == 0:
        return "null"

    # Deref WB and prefer dynamic type; also try non-synthetic view
    pointee = wb.Dereference()
    if not pointee or not pointee.IsValid():
        return None
    dyn = pointee.GetDynamicValue(lldb.eDynamicCanRunTarget)
    wrapper = dyn if dyn and dyn.IsValid() else pointee
    try:
        wrapper = wrapper.GetNonSyntheticValue() or wrapper
    except Exception:
        pass

    # Find Value on the wrapper (own or in bases)
    value = (_child_by_name_any(wrapper, ("Value", "value", "Value_", "value_"))
             or _find_field_recursive_any(wrapper, ("Value", "value", "Value_", "value_")))
    return value if value and value.IsValid() else None


def PolymorphicSummaryProvider(valobj, _dict):
    try:
        inner = _poly_inner_value(valobj)
        if inner == "null":
            return "std::nullopt"
        if not inner:
            return "<unavailable>"

        # Forward T's own summary if present; else clean fallbacks
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
    """Present Polymorphic<T> as if it were the inner Value."""

    def __init__(self, valobj, _dict):
        self.valobj = valobj
        self.inner = None
        self.update()

    def update(self):
        iv = _poly_inner_value(self.valobj)
        self.inner = None if (iv is None or iv == "null") else iv

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


# -------- NameInfo summary: "Prefix::Name" or "Name" --------

# --- helpers for NameInfo prefix chain (names only, no ids) ---

def _nameinfo_path_from(ni: lldb.SBValue) -> str | None:
    """Return 'A::B::C' built from NameInfo::Name along the Prefix chain. No ids."""
    if not ni or not ni.IsValid():
        return None
    # current name
    nm_v = _get_field_value_by_offset(ni, "Name") or ni.GetChildMemberWithName("Name")
    nm = _str_from_std_string(nm_v) if (nm_v and nm_v.IsValid()) else None
    if not nm:
        nm = "<no Name>"

    # recurse into Prefix (Polymorphic<NameInfo>)
    pref_poly = _get_field_value_by_offset(ni, "Prefix") or ni.GetChildMemberWithName("Prefix")
    if pref_poly and pref_poly.IsValid():
        inner = _poly_inner_value(pref_poly)  # "null" | SBValue(NameInfo) | None
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


# --- NameInfo summary: "Prefix::Name (idhex)" or "Name" ---

def NameInfoSummaryProvider(valobj, _dict):
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


def __lldb_init_module(debugger, _dict):
    cat = "MrDocs"
    # Register for both regexes; keep it boring and compatible.
    for rx in _INFO_REGEXES:
        debugger.HandleCommand(f'type summary add -w {cat} -x -P -F mrdocs_formatters.InfoLikeSummaryProvider "{rx}"')
        debugger.HandleCommand(f'type synthetic add -w {cat} -x -l mrdocs_formatters.InfoLikeSyntheticProvider "{rx}"')
    for rx in ("^clang::mrdocs::SymbolID$", "^const clang::mrdocs::SymbolID$"):
        debugger.HandleCommand(
            f'type summary add -w {cat} -x -P -F mrdocs_formatters.SymbolIDSummaryProvider "{rx}"'
        )
    for rx in ("^clang::mrdocs::Optional<.*>$", "^const clang::mrdocs::Optional<.*>$"):
        debugger.HandleCommand(
            f'type summary add -w {cat} -x -P -F mrdocs_formatters.OptionalSummaryProvider "{rx}"'
        )
        debugger.HandleCommand(
            f'type synthetic add -w {cat} -x -l mrdocs_formatters.OptionalTransparentSyntheticProvider "{rx}"'
        )
    for rx in ("^clang::mrdocs::Location$", "^const clang::mrdocs::Location$"):
        debugger.HandleCommand(
            f'type summary add -w {cat} -x -P -F mrdocs_formatters.LocationSummaryProvider "{rx}"'
        )
    for rx in ("^clang::mrdocs::Polymorphic<.*>$", "^const clang::mrdocs::Polymorphic<.*>$"):
        debugger.HandleCommand(
            f'type summary add -w {cat} -x -P -F mrdocs_formatters.PolymorphicSummaryProvider "{rx}"'
        )
        debugger.HandleCommand(
            f'type synthetic add -w {cat} -x -l mrdocs_formatters.PolymorphicTransparentSyntheticProvider "{rx}"'
        )
    for rx in (
            "^clang::mrdocs::NameInfo$",
            "^const clang::mrdocs::NameInfo$",
            "^class clang::mrdocs::NameInfo$",
            "^const class clang::mrdocs::NameInfo$",
            "^struct clang::mrdocs::NameInfo$",
            "^const struct clang::mrdocs::NameInfo$",
    ):
        debugger.HandleCommand(
            f'type summary add -w {cat} -x -P -F mrdocs_formatters.NameInfoSummaryProvider "{rx}"'
        )

    debugger.HandleCommand(f"type category enable {cat}")
