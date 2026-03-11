# Documentation Organization Plan

## Problem
145+ markdown files scattered across multiple directories with duplications and wrong information.

## Action Plan

### 1. Consolidate Incorrect Documentation
- [ ] Update `MASTER_DATABASE.md` with correct SetMasterDatabase behavior
- [ ] Mark outdated docs with "SUPERSEDED" headers
- [ ] Create single source of truth for API behavior

### 2. Organize by Topic

#### Proposed Structure:
```
docs/
├── api_surface/
│   ├── README.md (Master index)
│   ├── SETMASTERDATABASE.md (Corrected)
│   ├── GLOBAL_OBJECTS.md (All required globals)
│   ├── INITIALIZATION.md (Proper init sequence)
│   └── callbacks/ (Keep existing)
├── code_disassembly/
├── crashes/ (NEW - consolidate crash analysis)
│   ├── CRASH_ANALYSIS_TEMPLATE.md
│   └── CRASH_*.md
├── progress/ (NEW - timeline and status)
│   ├── MILESTONES.md
│   └── CURRENT_STATUS.md
└── resources/
```

### 3. Files to Update/Correct

**HIGH PRIORITY - WRONG INFO:**
- `MASTER_DATABASE.md` - Says to pass MasterDatabase, should pass NULL
- `INITIALIZATION_SEQUENCE.md` - Wrong initialization sequence

**HIGH PRIORITY - CONSOLIDATE:**
- Multiple CRASH_*.md files → One per unique crash
- Multiple PROGRESS/SUMMARY files → One timeline
- Duplicate callback docs → Keep only canonical versions

### 4. Create Index Files

Each directory needs:
- README.md explaining what's in that directory
- Index of all files with brief descriptions
- Links to related documentation

## Next Steps

1. Fix current crash
2. Create corrected MASTER_DATABASE.md
3. Create GLOBAL_OBJECTS.md with all required objects
4. Archive obsolete docs
5. Create proper index structure
