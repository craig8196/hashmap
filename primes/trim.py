#!/usr/bin/python3

import codecs
import bisect
from random import shuffle


with codecs.open("primes.txt", 'r') as f:
    with codecs.open("out.txt", 'w') as fout:
        primes = []
        for line in f:
            primes.extend(line.split())
            if len(primes) > 10000:
                shuffle(primes)
                primes = primes[0:30]
                primes = list(map(lambda p: int(p), primes))
                primes.sort()
                output_text = " ".join(map(lambda p: str(p), primes)) + "\n"
                fout.write(output_text)
                primes = []


