#include<stdio.h>
#include<assert.h>
#include<stdlib.h>
#include<string.h>

struct Person {
  char* name;
  int age;
  int height;
};

// Create a Person on the heap.
//
// Copies `name` argument into the result.
struct Person* Person_create(
    char* name,
    int age,
    int height)
{
  struct Person* person = malloc(sizeof(struct Person));
  person->name = strdup(name);
  person->age = age;
  person->height = height;
  return person;
}

// Deallocate a Person.
//
// Person "owns" thier name, so deallocate that as well
void Person_destroy(struct Person* person)
{
  assert(person != NULL);
  free(person->name);
  free(person);
}

void Person_show(struct Person* person)
{
  printf("Person {\n  \"%s\",\n  %d,\n  %d\n}\n",
      person->name, person->age, person->height);
}

int main(int argc, char* argv[])
{
  struct Person* betty = Person_create("Betty", 26, 61);
  struct Person* sarah = Person_create("Sarah", 24, 63);

  printf("Person betty lives at memory location %p:\n", betty);
  printf("Person sarah lives at memory location %p:\n", sarah);

  Person_show(betty);
  Person_show(sarah);

  betty->age = 27;
  printf("Betty had a birthday!\n");

  Person_show(betty);

  Person_destroy(betty);
  Person_destroy(sarah);

  return 0;

}
