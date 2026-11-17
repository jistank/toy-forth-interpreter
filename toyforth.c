#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include <string.h>

// DATA STRUCTURES
#define TFOBJ_TYPE_INT 0            // Type constant for Integer objects
#define TFOBJ_TYPE_STR 1            // Type constant for String objects
#define TFOBJ_TYPE_BOOL 2           // Type constant for Boolean objects
#define TFOBJ_TYPE_LIST 3           // Type constant for List objects

/*
 * Symbols we don't want to execute on the stack.
 * There is the table that connects the symbol's
 * name to a specific function
 */
#define TFOBJ_TYPE_SYMBOL 4         // Type constant for Symbol objects (function names, variables)

// TOY FORTH OBJECT
typedef struct tfobj {
    int refcount;
    int type;                       // Indicates which type in the union below is currently active
    union {                         // Union to save memory: only one of these members can be used at a time
        int i;                      // Value holder for TFOBJ_TYPE_INT or TFOBJ_TYPE_BOOL
        struct {
            char *ptr;              // Pointer to the actual string data
            size_t len;             // Length of the string
        } str;
        struct list {               // Structure for TFOBJ_TYPE_LIST
            struct tfobj **element; // Array of pointers to tfobj elements
            size_t len;             // Number of elements currently in the list
        } list;
    };
} tfobj;

typedef struct tfparser {
    char *prg;                      // Pointer to the entire program string to be compiled
    char *p;                        // Pointer to the current parsing position (next token to read)
} tfparser;


typedef struct tfctx {
    tfobj *stack;                   // The execution stack, represented as a Toy-Forth list object
} tfctx;

// ALLOCATION WRAPPERS
void *xmalloc(size_t size){
    void *ptr = malloc(size);       // Attempt to allocate the requested memory
    if (ptr == NULL){
        fprintf(stderr, "Out of memory allocating %zu bytes\n", size);
        exit(1);
    }
    return ptr;                     // Return the successfully allocated memory pointer
}

// Allocate and initialize a new Toy-forth object (used to create different object types)
tfobj *createObject(int type){
    tfobj *o = xmalloc(sizeof(tfobj));          // Allocate memory for the base object structure
    o->type = type;                             // Set the object's type discriminator
    o->refcount = 1;                            // Initialize reference count to 1 (newly created object is owned by the creator)
    return o;                                   // Return the newly created object
}

// OBJECT RELATED FUNCTIONS (the following functions allocate Toy-Forth objects of different types)

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

// LIST OBJECT

tfobj *createListObject(){
    tfobj *o = createObject(TFOBJ_TYPE_LIST);   // Create a base object of type List
    o->list.element = NULL;                     // Initialize the element array pointer to NULL
    o->list.len = 0;                            // Initialize the length to 0 (empty list)
    return o;                                   // Return the new list object
}

// Add the new element at the end of the list 'list'
// It is up to the caller to increment the reference count of the
// element added to the list, if needed
void listPush(tfobj *list, tfobj *ele){
    list->list.element = realloc(list->list.element, sizeof(tfobj*) * (list->list.len + 1));
    list->list.element[list->list.len] = ele;
    list->list.len++;
}

// TURN PROGRAM INTO TOY FORTH LIST

/*
 * Advances the parser's pointer past any whitespace characters.
 * Stops at the first non-whitespace character or the end of the string.
 */
void parserSpaces(tfparser *parser){

    /*
     * As long as the character I'm currently on is a space,
     * I increment the pointer and move on to the next character
     */
     while(isspace((unsigned char)parser->p[0]))
        parser->p++;
}

#define MAX_NUM_LEN 128

/*
 * Parses a sequence of digits (with an optional leading minus sign)
 * and creates a new Integer object from it.
 */
tfobj *parseNumber(tfparser *parser){
    char buf[MAX_NUM_LEN];
    char *start = parser->p;
    char *end;

    // Allow an optional negative sign
    if (parser->p[0] == '-')
        parser->p++;

    // Consume digits safely using unsigned char cast to avoid undefined behavior
    while(parser->p[0] && isdigit(parser->p[0]))
        parser->p++;

    end = parser->p;
    int numlen = end-start; // Pointer subtraction gives the exact character count

    // Prevent buffer overflow for excessively long numbers
    if (numlen >= MAX_NUM_LEN)
        return NULL;

    // Copy the number string into the local buffer and null-terminate it
    memcpy(buf, start, numlen);
    buf[numlen] = 0;

    // Convert the string to an integer and wrap it in a Toy-Forth object
    return createIntObject(atoi(buf));
}

/*
 * Compiles the raw program string into a Toy-Forth List of objects.
 * Iterates through the text, tokenizes it, and pushes valid objects to the list.
 */
tfobj *compile(char *prg){
    tfparser parser;
    parser.prg = prg;
    parser.p = prg;

    tfobj *parsed = createListObject();

    // Loop until we reach the null terminator of the string
    while(parser.p){
        tfobj *o;
        char *token_start = parser.p;

        parserSpaces(&parser);

        // Break if we reached the end of the string after skipping spaces
        if (parser.p[0] == 0)
            break;

        // Check if the current token is a number (starts with a digit or minus sign)
        if (isdigit (parser.p[0]) || parser.p[0] == '-'){
            o = parseNumber(&parser);
        } else {
            o = NULL; // Unknown token type, will trigger syntax error
        }

        // Handle syntax errors for unrecognized tokens
        if (o == NULL){
            printf("Syntax error near: %32s ...\n", token_start);
            return NULL;
        }

        listPush(parsed, o); // Add the successfully parsed object to our compiled list
    }
    return parsed;
}

// EXECUTE THE PROGRAM

/*
 * "Executes" the compiled program. 
 * Currently acts as a printer: iterates through the parsed objects 
 * and outputs their values to the console in a list format.
 */
void exec(tfobj *prg){
    printf("[");
    for (size_t j = 0; j < prg->list.len; j++){
        tfobj *o = prg->list.element[j];
        switch(o->type){
            case TFOBJ_TYPE_INT:
                printf("%d", o->i);
                break;
            default:
                printf("?");
                break;
        }
        printf(" ");
    }
    printf("]\n");
}

int main(int argc, char **argv){
    if (argc != 2){                             // Check if exactly one command-line argument (the filename) is provided
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    /*
     * Open the file (in read only text mode) passed from the cmd.
     * Returns a FILE pointer (NULL on failure).
     */
    FILE *fp = fopen(argv[1], "r");

    // Check whether fopen() failed
    if (fp == NULL){
        perror("Error opening file");
        return 1;
    }

    /* 
     * Move the file position indicator to the end of the file.
     * offset 0 relative to SEEK_END means "jump to the last byte".
     */
    fseek(fp, 0, SEEK_END);

    /* 
     * Return the current position of the file indicator, which now
     * corresponds to the total number of bytes in the file (its size).
     */
    long file_size = ftell(fp);

    /*
     * Allocate a buffer big enough to hold the whole file content, plus one
     * extra byte for the null terminator, so it can be used as a C string.
     */
    char *prgtext = xmalloc(file_size + 1);

    /*
     * Move the file position indicator back to the beginning of the file,
     * otherwise fread would start reading from the end and get nothing.
     */
    fseek(fp, 0, SEEK_SET);

    /*
     * Read the entire file into the buffer in a single block:
     * 1 element of size file_size bytes, read from fp into prgtext.
     */
    fread(prgtext, file_size, 1, fp);

    /*
     * Append the null terminator right after the last byte read,
     * turning the buffer into a valid C string.
     */
    prgtext[file_size] = 0;
    fclose(fp);

    tfobj *prg = compile(prgtext);
    exec(prg);

    return 0;
}

