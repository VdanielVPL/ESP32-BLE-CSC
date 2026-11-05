# pyright: reportUndefinedVariable=false
Import("env")

# NimBLE-Arduino: is_ble_hs_resolv_enabled type mismatch (library bug)
env.Append(LINKFLAGS=["-Wno-lto-type-mismatch"])
