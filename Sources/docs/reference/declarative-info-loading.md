# Declarative info loading (`CvInfoUtil`)

How `Cv*Info` classes load themselves from XML, and the recipe + pitfalls for migrating
the remaining hand-written ones. Tracking issue: **#196**.

## The model

Historically every info class hand-wrote four parallel methods that all had to agree
field-for-field: `read()` (XML → members), `copyNonDefaults()` (modular-merge), `getCheckSum()`
(OOS/Sync hash), and the constructor (defaults). Drift between them is a latent bug source.

`Sources/CvInfoUtil.h` replaces that with a single **declarative field registry**. A class
declares each XML-backed field once in `getDataMembers(CvInfoUtil& util)`, and `CvInfoUtil`
derives read / copy / checksum / init / link from that one declaration.

```cpp
void CvFooInfo::getDataMembers(CvInfoUtil& util)
{
    util
        .add(m_iCost, L"iCost")              // int / bool / float / CvString scalar
        .addEnum(m_eTechPrereq, L"PrereqTech")   // real enum FK (TechTypes&)
        .add(m_aeBonusTypes, L"BonusTypes")  // std::vector<EnumType> (flat <Root><Child>X</Child></Root>)
    ;
}

CvFooInfo::CvFooInfo()            { CvInfoUtil(this).initDataMembers(); }   // defaults
bool CvFooInfo::read(CvXMLLoadUtility* pXML)
{
    if (!CvInfoBase::read(pXML)) return false;   // (or CvHotkeyInfo::read, etc.)
    CvInfoUtil(this).readXml(pXML);
    return true;
}
void CvFooInfo::copyNonDefaults(const CvFooInfo* p) { CvInfoBase::copyNonDefaults(p); CvInfoUtil(this).copyNonDefaults(p); }
void CvFooInfo::getCheckSum(uint32_t& iSum) const   { CvInfoUtil(this).checkSum(iSum); }
```

`getDataMembers` is virtual on `CvInfoBase` (default empty), so the uniform parse-then-link
phase (`cvInternalGlobals::linkAllInfos()`) works on every info as it gets migrated;
un-migrated classes are a harmless no-op. `CvBuildInfo` is the fully-migrated reference.

## Wrapper catalog (what you can declare today)

| Call | For | Notes |
|------|-----|-------|
| `add(int/bool/float&, tag [,default])` | scalars | default defaults: `0`/`false`/`0.0f` |
| `add(CvString&, tag)` | strings | **not** checksummed (StringWrapper checksum is a no-op) |
| `add(char (&)[N], tag)` | fixed `char[N]` model strings | size N captured from the type |
| `addEnum(EnumType&, tag)` | real enum FK members | picks delayed/immediate resolution automatically |
| `addEnumAsInt(int&, tag)` | legacy `int` members holding a type index (read via `GetInfoClass`) | **immediate resolution only**, default `-1` |
| `add(std::vector<EnumType>&, tag)` | membership lists | reads flat list; merge = unique+sort |
| `add(IDValueMap<T1,int,d>&, tag)` | keyed value maps | `T1` must be an **info enum** (needs `InfoClassTraits<T1>`) |
| `add(IDValueMap<...array...>&, root, c1, c2)` | paired-array maps | |
| `addStruct(std::vector<Struct>&, root, elem)` | struct-vectors | `Struct` declares its own `getDataMembers`; nested FKs resolve immediately |
| `add(CvPropertyManipulators&)` | property manipulators | self-reads/checksums/merges |
| `addBoolExpr(const BoolExpr*&, tag)` | boolean expressions | **owns the heap expr** — drop the class's dtor `SAFE_DELETE` |
| `addYields(int*&, tag)` / `addCommerce(int*&, tag)` | `int[NUM_YIELD/COMMERCE_TYPES]` | **wrapper owns the array** — dtor must call `uninitDataMembers()` |

### Not yet supported (each blocks a cluster of classes)
1. **2D `m_ppi` arrays** (`int**`) — blocks Civic, Improvement, Property, Trait.
2. **Delayed-resolution enum-as-int** — `addEnumAsInt` is immediate-only, so an `int` FK that
   used `addDelayedResolution` (e.g. `CvVoteSourceInfo::m_iCivic`) can't use it yet.

**Before writing a new wrapper, check whether the data structure should exist at all.**
The former blocker "non-FK `IDValueMap` keyed by plain `int`" (`IDValueMapPercent`) was
resolved this way twice and is now extinct: `CvWorldInfo`'s `Percents` map held exactly one
ID across all data → replaced by the named scalar `iCityLimitsScalePercent` (#310), and the
`CvGameSpeedInfo`/`CvHandicapInfo` maps (#248) became `iUnitYieldScalePercent` + the
tag-dispatched `<Adapt>` grammar (handicap's map was dead data and was deleted outright) —
see `calendar-and-gamespeed.md`.
Owner ruling: **restructuring data into something descriptive and uniform beats preserving an
odd structure and special-casing the loader for it.**

## Migration recipe (per class)

1. Declare every XML-backed field in `getDataMembers` (see catalog).
2. Constructor → `CvInfoUtil(this).initDataMembers();` (keep only **non-XML runtime** fields in
   the init list, e.g. a GameFont `m_iChar` set later via `setChar`).
3. `read()` → base read + `CvInfoUtil(this).readXml(pXML);`
4. `copyNonDefaults()` → base copy + `CvInfoUtil(this).copyNonDefaults(p);`
5. `getCheckSum()` → `CvInfoUtil(this).checkSum(iSum);` (see pitfalls before delegating)
6. Build (`Tools/_Build.ps1 Assert build`), then load-verify: `Logs/XmlLoad.log` per-category
   count unchanged, **no** `Logs/Xml_MissingTypes.log`, no asserts.

## Checksum parity: what it buys and when to drop it

**What the info checksums are actually for** (traced 2026-06): every info's `getCheckSum`
folds into one asset checksum (`cvInternalGlobals::getAssetCheckSum`, logged per-info to
`Logs/Checksum.log`), which is stored in every savegame and compared on load
(`CvInitCore::checkVersions`). On mismatch the game offers the **modifier-recalculation
popup** — that is the checksum's *only* consumer. It does **not** feed multiplayer OOS
detection, and it never blocks loading.

**Owner ruling (2026-06-10): full parity between old and new structure is NOT required.**
The bar is that the game gets correct data; a uniform, descriptive structure beats
preserving a legacy quirk. The cost of a checksum change is exactly one spurious recalc
prompt per existing save — acceptable. So: restructure freely when the structure is the
problem, and don't contort `getDataMembers` to reproduce a legacy checksum.

Parity is still *useful* when it's free: an unchanged `Checksum.log` before/after a pure
migration proves the declarative path read identical data, and the first diverging info in
the log localizes a regression. Keep parity when you're only changing the loader, not the
data model — which is what the pitfalls below are for.

## Keeping it byte-identical (the pitfalls, for pure-loader migrations)

- **Declare in `getCheckSum` ORDER, not read order.** `checkSum()` iterates wrappers in
  declaration order; `readXml` order is irrelevant to values. CvString fields contribute nothing
  to the checksum, so park them anywhere (last is tidy).
- **Read-but-not-checksummed fields.** Some classes read a field the legacy checksum omits
  (`CvVictoryInfo::m_bTotalVictory`, `CvSpecialUnitInfo::m_bCityLoad`, `CvEspionageMissionInfo`'s
  last 8). Declaring it folds it into the delegated checksum → **changes the value**. Keep an
  explicit `getCheckSum` reproducing the legacy set (with a comment); migrate read/copy only.
- **`copyNonDefaults` default-mismatch bug.** Some hand-written copies compare an enum/enum-as-int
  FK against `0` instead of `-1` (e.g. `CvSpecialBuildingInfo`). The wrappers always use the
  type-correct default (`-1` for enums), so declarative copy **changes modular-merge behaviour**.
  Defer such classes or get a ruling (preserve-bug vs fix).
- **Runtime field that IS checksummed** (`CvInvisibleInfo::m_iChar`): keep it out of
  `getDataMembers`, checksum it explicitly *first*, then `CvInfoUtil(this).checkSum(iSum)`.
- **Array-owning classes:** `addYields`/`addCommerce`/`addBoolExpr` take ownership; switch the
  destructor from `SAFE_DELETE[_ARRAY]` to `CvInfoUtil(this).uninitDataMembers()`.
- All `NO_X` enum sentinels are `-1` (Civ4 convention), matching the enum wrappers' default.

## Status (2026-06)

`CvBuildInfo` fully migrated (reference). Incremental per-class migration in flight under #196;
29 classes migrated (incl. `CvWorldInfo` via the Percents→named-field restructure #310, and
`CvGameSpeedInfo` #248 via the calendar restructure — see `calendar-and-gamespeed.md`;
`CvEraInfo` is a partial adopter: its three calendar-pacing fields are declarative)
+ ~15 empty-stub sub-issues closed as no-work so far. Authoritative class
list = `EXPAND_FOR_EACH_INFO_CLASS` in `CvInfoClassTraits.h`. Related: prerequisite unification
is #195; the broader load-coherence background is in the bool-list flattening work.
