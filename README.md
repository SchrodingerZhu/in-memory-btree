# In-memory BTree

Nothing new, just as it is.

## Why did I write this?

Someone said btree is extremely slow in some cases, so I investigated.

## References

- Open Data Structures (opendatastructures.org)
- Rust `liballoc` (rust-lang/rust)

## Results
Btree factor = 6.
see `perf_btree.cpp`:

### Notice

Well, it was my fault that the result of `std::map` is discarded by compiler last time which makes the the results inaccurate.
So now we can see that btree wins `std::map` in nearly all aspects.

### Linear Search
```
10000000 insertions (map)
microsecs: 13949910
10000000 insertions (btree)
microsecs: 8337925
10000000 membership (map)
microsecs: 9943086
10000000 membership (btree)
microsecs: 3710939
10000000 erase min (map)
microsecs: 661049
10000000 erase min (btree)
microsecs: 628221
10000000 iterate through (map)
microsecs: 973423
10000000 iterate through (btree)
microsecs: 167454
```

### Binary Search
```
10000000 insertions (map)
microsecs: 11563843
10000000 insertions (btree)
microsecs: 7121332
10000000 membership (map)
microsecs: 9187248
10000000 membership (btree)
microsecs: 4910428
10000000 erase min (map)
microsecs: 651755
10000000 erase min (btree)
microsecs: 607065
10000000 iterate through (map)
microsecs: 944876
10000000 iterate through (btree)
microsecs: 168292
```

### Updadte for Copy Construction
I wrote a tail recursive copy constuctor for btree.
```
10000000 copy construct (map)
microsecs: 1108715
10000000 copy construct (btree)
microsecs: 442854
```

