#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Data structures
#define TFOBJ_TYPE_INT 0        // Type constant for Integer objects
#define TFOBJ_TYPE_STR 1        // Type constant for String objects
#define TFOBJ_TYPE_BOOL 2       // Type constant for Boolean objects
#define TFOBJ_TYPE_LIST 3       // Type constant for List objects

/*
 * Symbols we don't want to execute on the stack.
 * There is the table that connects the symbol's name to a specific function
 */
#define TFOBJ_TYPE_SYMBOL 4     // Type constant for Symbol objects (function names, variables)

// toy forth object
typedef struct tfobj {
    int refcount;
    int type;                   // Indicates which type in the union below is currently active
    union {                     // Union to save memory: only one of these members can be used at a time
        int i;                  // Value holder for TFOBJ_TYPE_INT or TFOBJ_TYPE_BOOL
        struct {
            char *ptr;          // Pointer to the actual string data
            size_t len;         // Length of the string
        } str;
        struct list {           // Structure for TFOBJ_TYPE_LIST
            struct tfobj **element; // Array of pointers to tfobj elements
            size_t len;             // Number of elements currently in the list
        } list;
    };
} tfobj;

typedef struct tfparser {
    char *prg;                  // Pointer to the entire program string to be compiled
    char *p;                    // Pointer to the current parsing position (next token to read)
} tfparser;


typedef struct tfctx {
    tfobj *stack;               // The execution stack
} tfctx;

// Allocation wrappers
void *xmalloc(size_t size){
    void *ptr = malloc(size);   // Attempt to allocate the requested memory
    if (ptr == NULL){
        fprintf(stderr, "Out of memory allocating %zu bytes\n", size);
        exit(1);
    }
    return ptr;                 // Return the successfully allocated memory pointer
}


// OBJECT RELATED FUNCTIONS (the following functions allocate Toy-Forth objects of different types)

// Allocate and initialize a new Toy-forth object
tfobj *createObject(int type){
    tfobj *o = xmalloc(sizeof(tfobj));          // Allocate memory for the base object structure
    o->type = type;                             // Set the object's type discriminator
    o->refcount = 1;                            // Initialize reference count to 1 (newly created object is owned by the creator)
    return o;                                   // Return the newly created object
}

tfobj *createStringObject(char *s, size_t len){
    tfobj *o = createObject(TFOBJ_TYPE_STR);    // Create a base object of type String
    o->str.ptr = s;                             // Assign the provided string pointer
    o->str.len = len;                           // Assign the provided string length
    return o;                                   // Return the new string object
}

tfobj *createSymbolObject(char *s, size_t len){
    tfobj *o = createStringObject(s, len);      // Correctly create a string object first to reuse its memory layout
    o->type = TFOBJ_TYPE_SYMBOL;                // Override the type to Symbol
    return o;                                   // Return the new symbol object
}

tfobj *createIntObject(int i){
    tfobj *o = createObject(TFOBJ_TYPE_INT);    // Create a base object of type Int
    o->i = i;                                   // Assign the integer value
    return o;                                   // Return the new int object
}

tfobj *createBoolObject(int i){
    tfobj *o = createObject(TFOBJ_TYPE_BOOL);   // Create a base object of type Bool
    o->i = i;                                   // Assign the boolean value (stored as int)
    return o;                                   // Return the new bool object
}

tfobj *createListObject(int i){
    tfobj *o = createObject(TFOBJ_TYPE_LIST);   // Create a base object of type List
    o->list.element = NULL;                     // Initialize the element array pointer to NULL
    o->list.len = 0;                            // Initialize the length to 0 (empty list)
    return o;                                   // Return the new list object
}

int main(int argc, char **argv){
    if (argc != 2){                             // Check if exactly one command-line argument (the filename) is provided
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;                               // Exit with an error
    }

     tfobj *prg = compile(prgtext);
     exec(prgtext);

    return 0;
}




















