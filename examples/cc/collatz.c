/* 考拉兹：27 变到 1 需要 111 步。返回步数 = 111 */
int main(){ int n=27; int s=0; while(n!=1){ if(n%2==0) n=n/2; else n=3*n+1; s=s+1; } return s; }
