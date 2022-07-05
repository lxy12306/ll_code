#include <stdio.h>
int main(){
    char * hellp = "Hello World!/n";
    printf("%p:%s\n",hellp, hellp);
    printf("%p:main", main);
    return 0;
}