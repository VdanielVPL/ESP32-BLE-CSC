# pyright: reportUndefinedVariable=false
Import("env")

# NimBLE-Arduino: niezgodny typ is_ble_hs_resolv_enabled (błąd biblioteki)
env.Append(LINKFLAGS=["-Wno-lto-type-mismatch"])
