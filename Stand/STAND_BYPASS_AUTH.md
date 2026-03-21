# `STAND_BYPASS_AUTH` / `STAND_DEV` — Complete Authentication Bypass Reference

> **Purpose:** This document describes every change required to fully bypass Stand's production authentication system for local development. The bypass is **not** a single toggle — it is a complete runtime chain replacement for all auth-dependent state transitions.

![Bypass Chain Overview](https://ibb.co/kN2LfNd)

---

## Table of Contents

1. [Production Auth Flow](#production-auth-flow)
2. [Bypass Chain Overview](#bypass-chain-overview)
3. [Developer Setup](#developer-setup)
4. [Bypass Implementation by File](#bypass-implementation-by-file)
   - [main.cpp — Early Init Fallback](#1-maincpp--early-init-fallback)
   - [Auth.cpp — verifyPermSig() Override](#2-authcpp--verifypermsigovr)
   - [get_root_list.cpp — GUI State Promotion](#3-get_root_listcpp--gui-state-promotion)
   - [RootNameMgr.cpp — RN_NULL Fallback](#4-rootnamemgrcpp--rn_null-fallback)
   - [Gui.cpp — Sig-Check Timeout](#5-guicpp--sig-check-timeout)
   - [Gui.cpp — Tutorial Gate](#6-guicpp--tutorial-gate)
   - [main.cpp — Bad State 3](#7-maincpp--bad-state-3)
   - [main.cpp — Perm Expiry & Heartbeat](#8-maincpp--perm-expiry--heartbeat)
5. [Runtime State Consistency Table](#runtime-state-consistency-table)
6. [Why Every Point Is Required](#why-every-point-is-required)
7. [Directive for Future Code Changes](#directive-for-future-code-changes)

---

## Production Auth Flow

```
STARTUP (main.cpp)
│
├─ discoverActivationKey()
│  ├─ Reads %APPDATA%\Stand\Activation Key.txt
│  ├─ Strips "Stand-Activate-" prefix → 31-char key → g_auth.activation_key_to_try
│  └─ If no file exists → key stays empty → initial_auth_done.fulfil() immediately
│
├─ Running loop: sendHeartbeat() (only if key is non-empty)
│  └─ processHeartbeatResponse()
│     ├─ verifyPermSig() validates RSA signature → returns permission level
│     ├─ g_auth.license_permissions = verified result
│     ├─ g_gui.unlock_recover_state = 1
│     ├─ RootNameMgr::current = RN_AUTHED (from "r" field in response)
│     └─ Queues gotPermSigJob()
│        └─ setApplicableNonFreeRootStateValue()
│           └─ unlock_recover_state transitions 1 → 2
│
├─ updateRootStateCore() → get_root_list() (get_root_list.cpp)
│  ├─ root_state = GUI_FREEONLINE
│  ├─ If key empty OR verifyPermSig() == FREE
│  │  └─ license_permissions = FREE
│  ├─ Else if unlock_recover_state != 0
│  │  └─ setApplicableNonFreeRootStateValue()
│  │     ├─ Reads license_permissions
│  │     └─ Sets root_state to matching edition (BASIC/REGULAR/ULTIMATE)
│  └─ RootNameMgr::getCurrentRootNameForState() → menu title
│
├─ PER-TICK checks (Gui.cpp onTick, main.cpp mainRunningLoopIteration)
│  ├─ Sig timeout: >20s since g_last_successful_sig_check → opened = false
│  ├─ Bad state 3: about_to_update_root_state stuck (race condition)
│  ├─ Bad state 24: unlock_recover_state == 0 when other players discovered
│  └─ Perm expiry: verifyPermSig() < license_permissions → logOut()
│
└─ CONSISTENCY (Auth.cpp isStateConsistentRaw)
   └─ GUI_ULTIMATE requires: !key.empty() && license_permissions == LICPERM_ULTIMATE
```

---

## Bypass Chain Overview

When `STAND_BYPASS_AUTH` or `STAND_DEV` is defined, the following 10-point chain must remain unbroken:

| # | Rule | File | What It Does |
|---|------|------|-------------|
| 1 | Activation Key Override | `main.cpp` | Injects 31-char dev key if no file exists |
| 2 | Permission Override | `Auth.cpp` | `verifyPermSig()` returns `LICPERM_ULTIMATE` |
| 3 | GUI Root State Promotion | `get_root_list.cpp` | Promotes `root_state` to `GUI_ULTIMATE` |
| 4 | Unlock Recovery | `get_root_list.cpp` | Forces `unlock_recover_state = 2` |
| 5 | Tunables Ready | `main.cpp` | `g_tunables.version = 1`, `ready.fulfil()` |
| 6 | Root Name Fallback | `RootNameMgr.cpp` | `RN_NULL` returns `stock_root_name` |
| 7 | Sig-Check Timeout | `Gui.cpp` | 20-second stale check compiled out |
| 8 | Bad State 3 | `main.cpp` | `about_to_update_root_state` check compiled out |
| 9 | Perm Expiry & Heartbeat | `main.cpp` | Expiry logout + heartbeat/tunables download compiled out |
| 10 | Tutorial Gate | `Gui.cpp` | Forces `Tutorial::state >= TUT_UP` on menu open |

---

## Developer Setup

### Activation Key File

Stand reads the activation key from:

```
%APPDATA%\Stand\Activation Key.txt
```

For development, create this file with the following exact content:

```
Stand-Activate-dev00000000000000000000000000000
```

**How it's parsed by `discoverActivationKey()`:**

1. `StringUtils::simplify()` lowercases and strips non-alphanumeric characters
2. Result: `standactivatedev00000000000000000000000000000`
3. Strip `"stand"` prefix → `activatedev00000000000000000000000000000`
4. Strip `"activate"` prefix → `dev00000000000000000000000000000`
5. Length check: 31 chars == `ACTIVATION_KEY_CHARS` ✓
6. Stored in `g_auth.activation_key_to_try`

The early-init bypass block only injects a fallback key if the file is **absent**. When the file is present, `discoverActivationKey()` handles it and the bypass only needs to set permissions, tunables, and auth-done.

### Build Configuration

Define one of the following in the project preprocessor definitions or `conf.hpp`:

```cpp
#define STAND_BYPASS_AUTH
// or
#define STAND_DEV
```

Both macros are treated identically throughout the codebase. All bypass guards use:
```cpp
#if defined(STAND_BYPASS_AUTH) || defined(STAND_DEV)
```

---

## Bypass Implementation by File

### 1. `main.cpp` — Early Init Fallback

**Location:** `mainAfterPointers()`, after `g_auth.discoverActivationKey()`

```cpp
g_auth.discoverActivationKey();
#if defined(STAND_BYPASS_AUTH) || defined(STAND_DEV)
    if (g_auth.activation_key_to_try.empty())
    {
        g_auth.activation_key_to_try = "dev" + std::string(28, '0');  // 31 chars
    }
    g_auth.license_permissions = LICPERM_ULTIMATE;
    g_tunables.version = 1;
    g_tunables.ready.fulfil();
    g_gui.initial_auth_done.fulfil();
#else
    if (g_auth.activation_key_to_try.empty())
    {
        g_gui.initial_auth_done.fulfil();
    }
#endif
```

**Critical design note:** The key injection is inside the `if (empty())` guard (only when no file exists), but permissions, tunables, and auth-done are **outside** the guard — they must always run in bypass mode regardless of whether the activation key file is present.

**What this does:**
- Injects a non-empty 31-char activation key (if needed)
- Sets `license_permissions = LICPERM_ULTIMATE` (backend auth state)
- Pretends tunables are loaded (`version = 1`, `ready.fulfil()`)
- Unblocks the loading screen (`initial_auth_done.fulfil()`)

**What this does NOT do:**
- It does **not** call `setApplicableNonFreeRootStateValue()` — the GUI root list isn't built yet at this point. Calling it here would crash (`ACCESS_VIOLATION at 0x0000000000000008`). The frontend promotion happens later in `get_root_list.cpp`.

---

### 2. `Auth.cpp` — `verifyPermSig()` Override

**Location:** `Auth::verifyPermSig()`

```cpp
LicensePermissions Auth::verifyPermSig() const
{
#if defined(STAND_BYPASS_AUTH) || defined(STAND_DEV)
    return LICPERM_ULTIMATE;
#else
    // Full RSA signature validation:
    // - Checks perm_expiry, perm_sig size, killswitch
    // - Verifies RSA signature against public key
    // - Updates g_last_successful_sig_check on success
    // - Returns LICPERM_FREE on any failure
    LicensePermissions perms = LICPERM_FREE;
    // ... RSA validation ...
    return perms;
#endif
}
```

**Why this matters:** `verifyPermSig()` is called from multiple locations:
- `get_root_list.cpp` — to determine initial GUI state
- `mainRunningLoopIteration()` — perm expiry check (compiled out in bypass)
- `processHeartbeatResponse()` — after receiving server response (compiled out in bypass)

The bypass has a dual guard:
- **Compile-time:** `#if defined(STAND_BYPASS_AUTH) || defined(STAND_DEV)`
- **Runtime (in `get_root_list.cpp`):** `activation_key_to_try.rfind("dev", 0) == 0`

---

### 3. `get_root_list.cpp` — GUI State Promotion

**Location:** Root state determination block inside `get_root_list()`

```cpp
if (g_auth.activation_key_to_try.empty()
    || g_auth.verifyPermSig() == LICPERM_FREE
    )
{
    g_auth.license_permissions = LICPERM_FREE;
}
#if defined(STAND_DEV) || defined(STAND_BYPASS_AUTH)
else
{
    g_auth.license_permissions = g_auth.verifyPermSig();   // → LICPERM_ULTIMATE
    g_gui.setApplicableNonFreeRootStateValue();             // → root_state = GUI_ULTIMATE
    if (g_gui.unlock_recover_state == 0)
    {
        g_gui.unlock_recover_state = 2;                     // Prevents bad state 24
    }
}
#else
else if (g_gui.unlock_recover_state != 0)
{
    g_gui.setApplicableNonFreeRootStateValue();
}
#endif
```

**This is where the frontend promotion happens.** It runs at root-list construction time, when GUI structures are fully initialized.

**What `setApplicableNonFreeRootStateValue()` does:**
```cpp
void Gui::setApplicableNonFreeRootStateValue() noexcept
{
    if (g_auth.license_permissions == LICPERM_BASIC)
        root_state = GUI_BASIC;
    else if (g_auth.license_permissions == LICPERM_REGULAR)
        root_state = GUI_REGULAR;
    else if (g_auth.license_permissions == LICPERM_ULTIMATE)
        root_state = GUI_ULTIMATE;
}
```

**Why `unlock_recover_state = 2` is needed:**
- Production flow: heartbeat → `unlock_recover_state = 1` → `gotPermSigJob()` → `unlock_recover_state = 2`
- Bypass flow: no heartbeat runs, so `unlock_recover_state` stays at `0`
- The sanity checker (`Sanity::discovered_other_player_non_free_without_recover_state`) triggers **bad state 24** if a non-free user has `unlock_recover_state == 0` when other players are found
- Setting it directly to `2` (the final "recovered" value) prevents this

---

### 4. `RootNameMgr.cpp` — `RN_NULL` Fallback

**Location:** `RootNameMgr::idToString()`

```cpp
case RN_NULL:
    return stock_root_name;
```

**Why this is needed:**
- In production, the heartbeat response's `"r"` field sets `RootNameMgr::current = RN_AUTHED`
- In bypass mode, no heartbeat runs, so `current` stays at `RN_NULL`
- Without this case, `idToString()` hits `SOUP_ASSERT_UNREACHABLE` and crashes
- `stock_root_name` provides a valid fallback title for the menu

---

### 5. `Gui.cpp` — Sig-Check Timeout

**Location:** `Gui::onTick()`

```cpp
#if !(defined(STAND_BYPASS_AUTH) || defined(STAND_DEV))
    // If >20 seconds since last successful signature check:
    //   g_gui.opened = false
    //   (forces menu closed)
#endif
```

**Why this is compiled out:**
- Production periodically refreshes `g_last_successful_sig_check` via `verifyPermSig()` during heartbeat responses
- In bypass mode, `verifyPermSig()` returns `LICPERM_ULTIMATE` immediately but **never updates `g_last_successful_sig_check`** (the update is in the `#else` branch)
- After 20 seconds, the stale check would force-close the menu

---

### 6. `Gui.cpp` — Tutorial Gate

**Location:** `Gui::inputOpenCloseMenu()`

```cpp
if (Input::keyboard_and_mouse && Tutorial::state < TUT_UP)
{
    Tutorial::setState(TUT_UP);
}
```

**Why this is needed:**
- The normal tutorial flow is tied to authentication state transitions
- In bypass mode, these transitions are altered, and the tutorial may not advance naturally
- Without this, keyboard/mouse input is blocked by the tutorial gate, causing a soft-lock when the menu opens
- This forces `Tutorial::state >= TUT_UP` to ensure navigation works immediately

---

### 7. `main.cpp` — Bad State 3

**Location:** `mainRunningLoopIteration()`, inside the bad-state checker

```cpp
#if !(defined(STAND_BYPASS_AUTH) || defined(STAND_DEV))
    if (g_gui.about_to_update_root_state)
    {
        bad_state = 3;
    }
    else
#endif
```

**Why this is compiled out:**
- `about_to_update_root_state` is a flag that detects a race condition during root-state transitions
- In bypass mode, the state transition timing is different from production
- This check can falsely interpret a legitimate bypass transition as corruption and trigger an unload

---

### 8. `main.cpp` — Perm Expiry & Heartbeat

**Location:** `mainRunningLoopIteration()`, inside the `!isUnloadPending()` block

```cpp
#if !(defined(STAND_BYPASS_AUTH) || defined(STAND_DEV))
    // Perm expiry logout:
    // if verifyPermSig() < license_permissions for 5 consecutive ticks → logOut()

    // Heartbeat & tunables download:
    // if key non-empty → download tunables if version==0, send heartbeat otherwise
#endif
```

**This single `#if` guard covers both:**

1. **Perm expiry logout** — Would revoke the session because there is no live server renewal in bypass mode.

2. **Heartbeat & tunables download** — The running loop would attempt to:
   - Download tunables from the real server (version mismatch with our fake `1`)
   - Send heartbeat with the dev key to the real auth server
   - The server response would overwrite bypass state (permissions, tunables, root name)
   - This was the cause of the "License Key is Invalid" bug — the heartbeat response conflicted with the bypass-injected state

**Key principle:** In bypass mode, all server data is hardcoded at init time. There is no server to talk to. No `/api/heartbeat`, no `/api/event`, no tunables download.

---

## Runtime State Consistency Table

For bypass mode to remain stable, **all** of these values must align:

| Variable / Condition | Required Value | Set Where |
|---|---|---|
| `activation_key_to_try` | `"dev" + 28×'0'` (31 chars) | `main.cpp` early init or `Activation Key.txt` |
| `license_permissions` | `LICPERM_ULTIMATE` (3) | `main.cpp` early init + `get_root_list.cpp` |
| `verifyPermSig()` return | `LICPERM_ULTIMATE` | `Auth.cpp` early return |
| `root_state` | `GUI_ULTIMATE` (6) | `get_root_list.cpp` via `setApplicableNonFreeRootStateValue()` |
| `unlock_recover_state` | `2` | `get_root_list.cpp` |
| `g_tunables.version` | `1` | `main.cpp` early init |
| `g_tunables.ready` | fulfilled | `main.cpp` early init |
| `g_gui.initial_auth_done` | fulfilled | `main.cpp` early init |
| `RootNameMgr::idToString(RN_NULL)` | `stock_root_name` | `RootNameMgr.cpp` |
| Sig-check timeout | compiled out | `Gui.cpp` |
| Bad state 3 check | compiled out | `main.cpp` |
| Perm expiry logout | compiled out | `main.cpp` |
| Heartbeat/tunables download | compiled out | `main.cpp` |
| `Tutorial::state` | `>= TUT_UP` | `Gui.cpp` |

---

## Why Every Point Is Required

The auth system is a chain of dependent state transitions. Missing any single point causes cascading failure:

| Missing Point | Failure Mode |
|---|---|
| No activation key | `isStateConsistentRaw()` fails → unload |
| `verifyPermSig()` not bypassed | Returns `LICPERM_FREE` → permissions reset |
| No GUI promotion | Backend says Ultimate, UI shows Free → bad state 5/7/8 |
| `unlock_recover_state` stays 0 | Bad state 24 when other players join |
| Tunables not ready | Loading screen never completes |
| `RN_NULL` not handled | `SOUP_ASSERT_UNREACHABLE` → crash |
| Sig timeout active | Menu force-closes after 20 seconds |
| Bad state 3 active | False corruption detection → unload |
| Perm expiry active | Logout after signature "expires" |
| Heartbeat still running | Server response overwrites bypass state → "Invalid Key" |
| Tutorial gate blocks input | Soft-lock on menu open |

---

## Directive for Future Code Changes

### Adding New Auth/Network Logic

Whenever adding new telemetry, license verification, anti-tamper, or network-dependent heartbeat logic, wrap it in:

```cpp
#if !(defined(STAND_BYPASS_AUTH) || defined(STAND_DEV))
    // production-only code
#endif
```

### State Alignment

Never modify `g_auth.license_permissions` or `g_gui.root_state` independently. They must transition together. If you change one, verify the other matches.

### No Server Calls in Bypass

Do not attempt to call `/api/heartbeat` or `/api/event` in bypass environments. Rely on the hardcoded `g_tunables` and `LICPERM_ULTIMATE` fallbacks.

### New Sanity Checks

If adding new anti-tamper or sanity checks in `mainRunningLoopIteration()`, you must explicitly account for the dev environment by either:
- Compiling the check out for dev builds, **or**
- Ensuring the dev state inherently passes the check

### Timing Sensitivity

The GUI state promotion (`setApplicableNonFreeRootStateValue()`) must happen in `get_root_list.cpp`, **not** in `main.cpp`. The GUI structures are not initialized until the script function runs. Calling it too early causes an `ACCESS_VIOLATION` (null pointer + 8-byte offset dereference).

---

## File Quick Reference

| File | Bypass Responsibility |
|---|---|
| `main.cpp` | Key injection, `LICPERM_ULTIMATE`, tunables, `initial_auth_done`, bad state 3, perm expiry, heartbeat compiled out |
| `Auth.cpp` | `verifyPermSig()` → returns `LICPERM_ULTIMATE` |
| `get_root_list.cpp` | `setApplicableNonFreeRootStateValue()`, `unlock_recover_state = 2` |
| `RootNameMgr.cpp` | `RN_NULL` → `stock_root_name` fallback |
| `Gui.cpp` | Sig-check timeout compiled out, tutorial gate on menu open |
| `%APPDATA%\Stand\Activation Key.txt` | `Stand-Activate-dev00000000000000000000000000000` |
