/* 斐波那契：递归。返回 fib(20) = 6765 */
int fib(int n){ if(n<2) return n; return fib(n-1)+fib(n-2); }
int main(){ return fib(20); }
