#!/bin/bash
# Download other hashmaps for benchmarking

curl "https://raw.githubusercontent.com/skarupke/flat_hash_map/master/bytell_hash_map.hpp" > test/bytell_hash_map.hpp
curl "https://raw.githubusercontent.com/skarupke/flat_hash_map/master/flat_hash_map.hpp" > test/flat_hash_map.hpp
curl "https://raw.githubusercontent.com/skarupke/flat_hash_map/master/unordered_map.hpp" > test/unordered_map.hpp

