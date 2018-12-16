#!/usr/bin/python3

import codecs
import bisect


primes = []
with codecs.open("primes.txt", 'r') as f:
    primes = f.read().split()

primes = list(map(lambda p: int(p), primes))
primes.extend([3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61])
nslots = 4.0
primes.sort()
biggest = max(primes)

MAX_INT = (2**31) - 1
LOG2 = 31
MAX_BUCKETS = (MAX_INT//4) - 31
MAX_SLOTS = (MAX_BUCKETS * 4)

final_list = []

while nslots < MAX_SLOTS:
    #print(nslots)
    nbuckets = int(nslots)//4
    iprime = bisect.bisect(primes, nbuckets)
    if iprime >= len(primes):
        break
    #print('Primes: [%s, %s]'%(str(primes[iprime - 1]), str(primes[iprime])))
    final_list.append(primes[iprime])
    # nslots = nslots * 1.50
    nslots = (nslots * 7.0) / 5.0

final_list.append(1)
final_list = list(set(final_list))
final_list.sort()
print(final_list)

for i, prime in enumerate(final_list):
    print('    %s,'%(str(prime)))
for i, prime in enumerate(final_list):
    print('    case %s: index = index %% %s; break;'%(str(i), str(prime)))


nslots = 1.0
while nslots < MAX_SLOTS:
    nbuckets = int(nslots)//4
    print(nbuckets)
    nslots = (nslots * 7.0) / 5.0

