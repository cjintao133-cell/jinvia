/* 欧几里得求最大公约数。返回 gcd(1071,462) = 21 */
int gcd(int a,int b){ while(b!=0){ int t=b; b=a%b; a=t; } return a; }
int main(){ return gcd(1071,462); }
