# pyright: reportUndefinedVariable=false
Import("env")

# NimBLE-Arduino 2.5.1: niezgodny typ is_ble_hs_resolv_enabled (blad biblioteki)
env.Append(LINKFLAGS=["-Wno-lto-type-mismatch"])
