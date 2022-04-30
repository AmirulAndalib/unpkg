Usage:
    unpkg <input.pkg> <directory>

Makefile:
    make pbzx pbzy pbzz

pbzx:
    decompressor for apple archives with magic header `pbzx` (cross-platform)
pbzy:
    decompressor for apple archives with magic header `pbze`, `pbz4`, `pbzx` or `pbzz` (auto multi-thread)
pbzz:
    decompressor for apple archives with magic header `pbze`, `pbz4` or `pbzx` (single-thread)
