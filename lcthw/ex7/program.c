#include <stdio.h>

int main(int argc, char* argv[])
{
  int distance = 100;
  float power = 2.35f;
  double super_power = 9420.29;
  char initial = 'L';
  char first_name[] = "Steven";
  char* last_name = "Troxler";

  printf("I have %d miles to go\n", distance);
  printf("My power is %f, and superpower is %f\n", power, super_power);
  printf("My name is %s %c %s\n", first_name, initial, last_name);

  double small_number = 0.003 / 10499150.0;
  printf("A small number: %e\n", small_number);

  char nul_byte = '\0';
  printf("The null byte, as an int, is %d. That's 100%% cool!\n", nul_byte);
}
