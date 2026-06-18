#!/usr/bin/env python3
"""Test 2: Dynamic sub-column discovery.

Takes mock data dict with 11-tuple keys, discovers unique (model, hermes) combos,
orders them: Base first -> OCP modes -> Forward models.
Returns ordered list + column index mapping.
"""

# Mock data keys: (cores, trace, pf, repl, l1byp, l2byp, l3byp, ocp, ...)
# For sub-col discovery we care about: bypass_model + ocp_mode
# "Base" = model="no", ocp="off"
# OCP modes = model="no", ocp in {hermes, ttp, hmp}
# Forward models = model != "no", ocp="off"

mock_data = {
    # (cores, trace, pf, repl, l1m, l2m, l3m, ocp, warmup, sim, extra)
    (1, "605.mcf", "no",      "lru", "no", "no", "no", "off",     0, 0, ""): {"ipc": 1.20},
    (1, "605.mcf", "no",      "lru", "no", "no", "no", "hermes",  0, 0, ""): {"ipc": 1.18},
    (1, "605.mcf", "no",      "lru", "no", "no", "no", "ttp",     0, 0, ""): {"ipc": 1.15},
    (1, "605.mcf", "berti_c", "lru", "no", "no", "no", "off",     0, 0, ""): {"ipc": 1.50},
    (1, "605.mcf", "berti_c", "lru", "no", "no", "no", "hermes",  0, 0, ""): {"ipc": 1.48},
    (1, "605.mcf", "berti_c", "lru", "4000fix", "4000fix", "4000fix", "off", 0, 0, ""): {"ipc": 1.55},
    (1, "429.bzip", "no",     "lru", "no", "no", "no", "off",     0, 0, ""): {"ipc": 2.00},
    (1, "429.bzip", "berti_c","lru", "no", "no", "no", "off",     0, 0, ""): {"ipc": 2.40},
    (1, "429.bzip", "berti_c","lru", "4000fix", "4000fix", "4000fix", "off", 0, 0, ""): {"ipc": 2.50},
}

# Key indices (matching real aggregator convention)
IDX_L1M = 4
IDX_L2M = 5
IDX_L3M = 6
IDX_OCP = 7

def discover_subcols(data):
    """Discover unique (bypass_model_tag, ocp) combos and order them."""
    combos = set()
    for key in data:
        # bypass model tag: use l2 model as representative (they're usually same)
        byp = key[IDX_L2M]
        ocp = key[IDX_OCP]
        combos.add((byp, ocp))

    # Classify
    base = []
    ocp_modes = []
    fwd_models = []

    for byp, ocp in combos:
        if byp == "no" and ocp == "off":
            base.append((byp, ocp))
        elif byp == "no" and ocp != "off":
            ocp_modes.append((byp, ocp))
        else:
            fwd_models.append((byp, ocp))

    # Sort within groups
    ocp_modes.sort(key=lambda x: x[1])
    fwd_models.sort(key=lambda x: x[0])

    ordered = base + ocp_modes + fwd_models

    # Build col index mapping (0-based within a trace group)
    col_map = {combo: idx for idx, combo in enumerate(ordered)}

    return ordered, col_map


def make_label(byp, ocp):
    """Human-readable sub-col label."""
    if byp == "no" and ocp == "off":
        return "Base"
    elif byp == "no":
        return ocp.upper()
    else:
        return byp


ordered, col_map = discover_subcols(mock_data)

print("Ordered sub-columns:")
for i, (byp, ocp) in enumerate(ordered):
    label = make_label(byp, ocp)
    print(f"  [{i}] ({byp}, {ocp}) -> \"{label}\"")

print(f"\nCol map: {col_map}")
print(f"Total sub-cols per trace: {len(ordered)}")

# Verify Base is index 0
assert ordered[0] == ("no", "off"), f"Base not first! Got {ordered[0]}"
assert col_map[("no", "off")] == 0, "Base not at index 0"
print("\nAll assertions passed.")
