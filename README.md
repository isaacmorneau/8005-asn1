# 8005-asn1

A simple program to demonstrait the performance differences between threads processes and OpenMP

## Build 

```
$ cmake ./
$ make
```

## Usage

```
	 -[-o]penmp                    - run task with openmp
	 -[-p]rocess                   - run task with processes
	 -[-t]hread                    - run task with threads
	 -[-b]ranch      <default 100> - number of times to calculate pi
	 -[-i]tterations <default 1>   - number of times to run to get time samples
	 -[-h]elp                      - this message
```
