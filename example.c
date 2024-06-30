#include <stdio.h>
#include <stdbool.h>

int main() {

    // This assigns the variable to a new value which is 10
    int myNum = 15;
    myNum = 10;

    // This shows the usage of format specifiers
    // %d is used for integers and %c is used for characters
    char myLetter = 'D';
    printf("My number is %d and my letter is %c", myNum, myLetter); 

    // Student data
    int studentID = 15;
    int studentAge = 23;
    float studentFee = 75.25;
    char studentGrade = 'B';

    // Print variables
    printf("Student id: %d\n", studentID);
    printf("Student age: %d\n", studentAge);
    printf("Student fee: %f\n", studentFee);
    printf("Student grade: %c", studentGrade);

    char myText[] = "Hello";
    printf("%s", myText);

    float f1 = 35e3;
    double d1 = 12E4;

    printf("%f\n", f1);
    printf("%lf", d1);

    int myInt;
    float myFloat;
    double myDouble;
    char myChar;

    printf("%lu\n", sizeof(myInt));
    printf("%lu\n", sizeof(myFloat));
    printf("%lu\n", sizeof(myDouble));
    printf("%lu\n", sizeof(myChar));

    printf("");

        // Create boolean variables
    bool isProgrammingFun = true;
    bool isFishTasty = false;

    // Return boolean values
    printf("%d\n", isProgrammingFun);   // Returns 1 (true)
    printf("%d\n", isFishTasty);        // Returns 0 (false)

    printf("%d", 10 == 10); // Returns 1 (true), because 10 is equal to 10
    printf("%d", 10 == 15); // Returns 0 (false), because 10 is not equal to 15
    printf("%d\n", 5 == 55);  // Returns 0 (false) because 5 is not equal to 55


    int time = 22;
    if (time < 10) {
        printf("Good morning.\n");
    } else if (time < 20) {
        printf("Good day.\n");
    } else {
        printf("Good evening.\n");
    }

    return 0;
}