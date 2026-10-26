#include <stdio.h>
#include <stdlib.h>

/* IMPLEMENT ME: Declare your functions here */

typedef int (*Operation)(int, int);
int add(int a, int b);
int subtract(int a, int b);
int multiply(int a, int b);
int divide(int a, int b);
int quit_program(int a, int b);

int main (void)
{
    int a = 6;
    int b = 3;
    char choice;

    Operation operations[] = {
        add,
        subtract,
        multiply,
        divide,
        quit_program
    };

printf("Operand 'a' : %d | Operand 'b' : %d\n", a, b);
printf("Specify the operation to perform "
       "(0 : add | 1 : subtract | 2 : Multiply | 3 : divide | 4 : exit): ");
          
    scanf("%c", &choice);

    int index = choice - '0';

    printf("x = %d\n", operations[index](a, b));

    return 0;
};

/* IMPLEMENT ME: Define your functions here */
int add (int a, int b) { printf ("Adding 'a' and 'b'\n"); return a + b; }
int subtract (int a, int b) { printf ("Subtracting 'b' from 'a'\n"); return a - b; }
int multiply (int a, int b) { printf ("Multiplying 'a' and 'b'\n"); return a * b; }
int divide (int a, int b) { printf ("Dividing 'a' by 'b'\n"); return a / b; }
int quit_program(int a, int b)
{
    exit(0);
    return 0;
}
