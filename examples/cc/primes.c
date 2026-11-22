/* 数素数：统计 < 100 的素数个数 = 25 */
int isprime(int n){ int d=2; if(n<2) return 0; while(d*d<=n){ if(n%d==0) return 0; d=d+1; } return 1; }
int main(){ int c=0; int i=2; while(i<100){ c=c+isprime(i); i=i+1; } return c; }
