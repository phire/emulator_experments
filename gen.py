#!/usr/bin/env python2

def eq(f):
	return f & 4

def ne(f):
	return not f & 4

def cs(f):
	return f & 2

def cc(f):
	return not f & 2

def mi(f):
	return f & 8

def pl(f):
	return not f & 8

def vs(f):
	return f & 1

def vc(f):
	return not f & 1

def hi(f):
	return f & 2 and not f & 4

def ls(f):
	return not f & 2 and f & 4

def ge(f):
	return f & 8 == f & 1

def lt(f):
	return f & 8 != f & 1

def gt(f):
	return f & 4 == 0 and f & 8 == f & 1

def le(f):
	return f & 4 or f & 8 != f & 1

def al(f):
	return True

def nv(f):
	return False

conditions = [eq, ne, cs, cc, mi, pl, vs, vc, hi, ls, ge, lt, gt, le, al, nv]

print "dw ",

for i in range(0x10):
	val = 0
	for n,c in enumerate(conditions):
	#	print i, n, int(c(i)!=0)
		val |= int(c(i)!=0) << n
	print hex(val) + ",",
	#print hex(i) + ": " + hex(val)

print ""

