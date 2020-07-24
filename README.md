# In-memory BTree

Nothing new, just as it is.

## Why did I write this?

Someone said btree is extremely slow in some cases, so I investigated.

## References

- Open Data Structures (opendatastructures.org)
- Rust `liballoc` (rust-lang/rust)

## Results

see `perf_btree.cpp`:

```
10000000 insertions (map)
microsecs: 12554578
10000000 insertions (btree)
microsecs: 9709360
10000000 membership (map)
microsecs: 182448
10000000 membership (btree)
microsecs: 5104937
10000000 erase min (map)
microsecs: 690931
10000000 erase min (btree)
microsecs: 389580
```
Btree factor = 6.
(binary search and linear search are similar)
Does anyone know why btree search is that slow?
