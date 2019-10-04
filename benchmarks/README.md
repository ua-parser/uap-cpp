Performance benchmarks
======================

Results of `make bench` on two different machines:

| Processor            | Compiler            | Real time | User CPU time | System CPU time |
| -------------------- | ------------------- | --------- | ------------- | --------------- |
| Intel Core i7 2.2GHz | AppleClang 10.0.1   | 39.57     | 39.50         | 0.04            |
| Intel N3700 1.6GHz   | GCC 8.3             | 98.79     | 98.75         | 0.02            |

The benchmarks use a realistic set of user agent strings, parsed 1000 times each (to make the numbers more reliable, and to offset the initial setup).
