/* 传感器量程翻倍：读一次温度(read:temp=34)再乘 2 = 68。演示 C 调用宿主能力。 */
extern int read_temp(int);
int main(){ return read_temp(0) * 2; }
