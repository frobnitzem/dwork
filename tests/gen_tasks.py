from functools import *

def mkStencil(N, T):
    assert T > 0
    assert N > 1

    print("\n".join(map(str, range(N))))
    for t in range(1, T):
        n = t*N
        print( "%d,%d,%d"%(n,n-N,n-N+1) )
        for n in range(t*N+1,t*N+N-1):
            print( "%d,%d,%d,%d"%(n,n-N-1,n-N,n-N+1) )
        n = (t+1)*N-1
        print( "%d,%d,%d"%(n,n-N-1,n-N) )

def main(argv):
    assert len(argv) == 3, "Usage: %s <N> <T>"%argv[0]
    N = int(argv[1])
    T = int(argv[2])
    mkStencil(N, T)

if __name__=="__main__":
    import sys
    main(sys.argv)
