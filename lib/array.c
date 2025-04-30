#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
// TODO: experiments
// #include <sys/resource.h>


//	TODO:
//		- Use struct for namespacing and
//		"Global config" if possible.
//
//		- support for other HC headers 
//			array_from_list(List l) -> Array 
//			array_from_string(String) -> Array
//
// IMPORTANT:
//	! [partial] add FULL support for flags:
//
//	- make a `consumed` field in struct, that
//	will cause assert on attempt of using Array
//	marked as consumed. Use it with `ARRAYFLAG_FREE_AFTER_CLONE`
//
//	- measure performance, make shure 
//		its isn't much worse then usual
//		array oprations
//
//	- add functions:
//		[v] array_dup(array) 				-> Array
//		[v] array_rebase(array*) 			-> void
//		[v] array_pinch(array, index)		-> owned array[index]
//		[v] array_remove_last(array) 		-> Array[last]
//		[v] array_last(array) 				-> Array[last]
//		[v]	array_setflags(array*,flags)	-> void
//		[v] array_merge(a1*,a2*)			-> Array
//		[v] array_split(r1,r2,array,index)	-> void
//			-- array_slice(array, begin, end) 		-> Array
//			// TODO: add silice type for 
//			// multiheader use by HCH
//		[.] array_chop (array, begin, end) 		-> Array
//		[v] array_swap(array*,l,r)			-> void

// Header guard

#ifndef HCH_ARRAY_H
#define HCH_ARRAY_H

typedef int array_bitmask;

typedef enum {
	
	// Automaticall grows the array
	// when inserting outside of it.
	ARRAYFLAG_AUTOGROW				= 1,	// b00000001
	
	// Atomatically shrink on deletion of elements
	// or rebase calls.
	ARRAYFLAG_AUTOSHRINK			= 2,	// b00000010
	

	// frees the children items automatically
	// assuming they are pointers:
	// 	- on deletion
	// 	- on clear
	// 	- on rebase
	// 	NOTE: if item IS NOT A POINTER ALLOCATED
	// 	VIA GIVEN ALLOCATOR, THIS WILL TRIGGER CRASH
	//
	// 	to use on structs or other complex data, use it
	// 	with ARRAYFLAG_USE_DESTRUCTOR
	ARRAYFLAG_AUTOFREE_ITEM_CELLS	= 4,	// b00000100

	// Will make array expect a pointer
	// to Array_Destructor function, which 
	// replaces all internal ARRAY_FREE calls.
	//
	// This meant for any type of data that isn't
	// just a pointer, but rather a complex struct
	// or an array.
	//
	// USING THIS OPTION INTERACTS WITH:
	// 	-	ARRAYFLAG_AUTOFREE_CHILDREN
	// 	-	ARRAYFLAG_SMART_STACK_HANDLER
	// 	-	ARRAYFLAG_SAFE_INSERTION
	ARRAYFLAG_USE_DESTRUCTOR		= 8,	// b00001000
	
	// By defining this Array either uses
	// already included "memory.h" strack tracker
	// or uses build in one. 
	// NOTE: this has to be enabled via macro:
	// 	ARRAY_STACK_TRACKER
	//
	// Stack tracker on creation in the begining 
	// of your main, will take size of the stack
	// of currently run program and calculate the 
	// pointer of its end.
	//
	// Thus every time when seeing a pointer in
	// any of the children, it will check if pointer
	// belongs to stack or outside source, and call 
	// user defined callback function to free it if 
	// such exists, otherwise just calls ARRAY_FREE.
	//
	// Stack autotracker allows mixing stack-created items
	// and dynamically allocated one.
	ARRAYFLAG_SMART_STACK_HANDLER	= 16,	// b00010000
	
	// This is a function for handling situation when 
	// you insert into already existing element of the
	// array wanting to OVERWRITE it.
	//
	// The array will call given by you Array_Destructor
	// function to clear the struct/pointer/anything that 
	// is put in the array cell, if THIS OPTION IS USED.
	//
	// If option is defined and no destructor function is given,
	// then any overwrite insertion in the array will assert 
	// with ARRAY_OVERWRITE error.
	//
	// If option is not defined, then it will have overwrite
	// behaviour, which with structs that have allocated 
	// data bound to pointer, will cause a memory leak.
	ARRAYFLAG_SAFE_INSERTION		= 32,	// b00100000
	
	// Any action that may produce two or more different
	// Arrays, or do any duplicates of it
	// with this flag will free the "input(s) array".
	//
	// this means function like:
	// 	- array_merge
	// 	- array_chop
	// 	- array_split
	// 	
	// 	will free input arrays, instead of leaving them as is.
	//
	// 	This flag in case of merging has to be enabled
	// 	in BOTH input arrays
	ARRAYFLAG_FREE_AFTER_CLONE		= 64,	// b01000000
											  
    // Makes any array_free call act very pedantic
	// causing assert on double-frees, 
	// by disabling the safety check
	// on (data) and (occupied) pointers.
	//
	// Exists in cases when a pedantic explicitness is needed
	// in memory managment functions.
	ARRAYFLAG_PEDANTIC_FREE			= 128,	// b10000000
											
} ArrayFlags; // NOTE:
			  // 	By default array doesn't have any flags
			  // 	this means you are responsible for
			  // 	all the memory managment and 
			  // 	resizing of the array.
			  //
			  // 	THOSE OPTIONS ARE JUST QoL
			  // 	and should be used only when 
			  // 	its clear what they do.

typedef void (*ArrayDestructor) (void*);

typedef struct {
	ArrayDestructor destroy;
	array_bitmask 	flags;
	size_t 			capacity;
	size_t			count;
	size_t			item_sz;
	char*			occupied; 
	void*			data;
	void*			scratch_buffer;
} Array;

#ifndef ARRAY_MALLOC
#	define ARRAY_MALLOC malloc
#endif

#ifndef ARRAY_FREE
#	define ARRAY_FREE free
#endif

// DECLARATIONS AND DOCS

	// Inits the array of set capacity for a given
	// Type inside of the provided Array instance pointer
	//
	// Doesnt return anything
#define array_init(ARRAY,CAP,TYPE) \
	IGNORE_RETURN __array_init(ARRAY,CAP,sizeof(TYPE))
	
	// Creates new array of ARRAY_INITIAL_CAPACITY for a
	// given type and returns to user a stack instance of Array
#define array_new(TYPE) \
	__array_init(NULL,ARRAY_INITIAL_CAPACITY,sizeof(TYPE))


	// Creates array from given pointer/C fixed array with length of (LENGTH)
	// items, which are input data, since library isn't responisble for guessing
	// what array type is given input.
	//
	// Function only fails if length of the array 0, item is of size 0, which i belive
	// can only be type (void) and of array pointer is not valid.
	//
	// Returns new Array struct as a product.
#define array_from(ARRAY,LENGTH) 	\
	__array_from((ARRAY),sizeof((ARRAY)[0]),(LENGTH))

	// inserts item at index, potentially by overwriting the copy of it
	// unless the ARRAYFLAG_SAFE_INSERTION is specified, in this case
	// the callback "destruction" function is called that 
	// frees object properly without causing a memory leak
	//
	// if index is greater then count of items, inserts item at index 
	// and sets count ot be of the index+1
	//
	// if no space is left and ARRAYFLAG_AUTOGROW is not specified
	// asserts overflow error.
	// otherwise grows the array by twice of its capacity and 
	// inserts the item after.
#define array_insert(ARRAY,INDEX,ITEM_PTR) \
	__array_insert((ARRAY), (INDEX), (ITEM_PTR), sizeof((ITEM_PTR)))

	// appends given item to the first avilable cell in the array.
	// iteration happens from beginig linearly
	// acts like array_insert but instead of expecting index from
	// user, linearly searches for first cell marked as "empty"
#define array_append(ARRAY,ITEM_PTR) \
	__array_insert((ARRAY),__array_append_index((ARRAY)),(ITEM_PTR), sizeof((ITEM_PTR)))


	// Sets the flags for special behaviour
void array_setflags(Array* a, array_bitmask mask);

	// Clears array items by memset-ing them to 0,
	// resets the count of items in array.
	//
	// if ARRAYFLAG_AUTOFREE_CHILDREN is defined
	// calls "destructor" function on each of elements 
	//
	// later used in `array_resize` when `new_size` is 0
void 	array_clear(Array* a);

	// resizes the array of items to a new size.
	// when array is resized to lower capacity, calls
	// and ARRAYFLAG_AUTOFREE_CHILDREN is defined, will call 
	// a "destructor" function on each "lost" item
	//
	// when new_size is the same as array capacity - does nothing.
	// when new_size is 0 - calls array_clear() instead of doing reallocations;
void 	array_resize (Array* a, size_t new_size);

	// Frees the array items and "occupied" indexing
	// sets entire structure to 0.
	//
	// Array WILL require another initialization and 
	// configuring for proper reuse.
void 	array_free(Array* a);
	
	// Return read-only pointer to array data
	// expected to be used like this:
	// 		Array arr = array_new(MyType);
	// 		MyType* item = array_raw(arr);
const void* array_raw(Array a);

	// Return array capcity
size_t 	array_cap(Array a);

	// Return array item count
size_t 	array_len(Array a);

	// Returns a pointer to item at given index of the array 
	// expected to be used like this: 
	// 		Array arr = array_new(MyType);
	// 		MyType* item = array_refer(arr);
	// 	
	// 	if asked index lies outside of the array, 
	// 	asserts with overflow error.
void* array_refer(Array a, size_t index);

	// Returns derefenced value of the array_refer() return value.
	// A shortcut for getting copy of the value assigned to a new
	// variable on the stack or other similar use case.
#define array_get(ARRAY, INDEX, TYPE) \
	*((TYPE*)array_refer((ARRAY),(INDEX)))


	// gets last element
	// just a wrapper around array_refer
	// same as array_get
#define array_last(ARRAY,TYPE) \
	*((TYPE*)array_refer((ARRAY),(ARRAY).count-1))

	// Removes item at given index.
	//
	// if index lies outside of the array -
	// asserts with overflow error.
	//
	// Unlike other array item functions ignores both:
	// ARRAYFLAG_AUTOGROW and ARRAYFLAG_AUTOSHRINK
void array_remove(Array* a, size_t index);

	
	// removes last element
	// just a wrapper around array_remove
#define array_remove_last(ARRAY) \
	array_remove((ARRAY),((ARRAY)->count)-1)


	// Iterates over (count) of items in the (array)
	// every instance of item is refered to via 
	// 		(ITER) pointer,
	// that you define as second argument
	//
	//
	// used like this:
	// 		int items[4] = {1,2,3,4};
	// 		Array arr = array_from(items,4);
	//		array_foreach(arr, int* item,
	//  		printf("[%i] ",*item);
	//  	);
	//
	//  also reverse iteration foreach exists:
	//  	array_eachfor(ARRAY,ITER,...)
	//  	yeah, funny naming convention. i guess
	//
	//		array_eachfor(arr, int* item,
	//			printf("[%i] ",*item)
	//		);
#define array_foreach(ARRAY,ITER,...) do { 			\
	for(size_t i = 0; i < (ARRAY).count; i++) {		\
		ITER	=	array_refer((ARRAY),i);			\
		if (1) {__VA_ARGS__}						\
	}} while(0);									\


#define array_eachfor(ARRAY,ITER,...) do { 			\
	for(size_t i = ((ARRAY).count); i > 0; i--) {\
		ITER	=	array_refer((ARRAY),i-1);			\
		if (1) {__VA_ARGS__}						\
	}} while(0);									\


	// Creates and array that has the same 
	// capacity, flags as input, but 0 items
Array array_inherit(Array a);

	// Always duplicates the array with all its
	// flags and items. 
Array array_dup(Array a);

	// Swaps to elements of the array at given 
	// indexes for left and right element
void array_swap(Array* a, size_t l, size_t r);

	// Removes all the empty cells in-between
	// real items by modifying the input array
	// Essentially used to "defragement" the array
	// when it has way too many gaps.
void array_rebase(Array* a);

	// Removes the element at a given index, shifts
	// all the items to cover the gap and 
	// return a pointer to allocated copy of it 
	// Copy has to be freed after being used
void* array_pinch(Array* a, size_t index);

	// Return a new array formed from (l) and (r)
	// with sum of thier capacity and item count
	//
	// if ARRAYFLAG_FREE_AFTER_CLONE is defined,
	// frees the (l) and (r) after constructing
	// the merged array.
	//
	// if sizes of data inside the (l) and (r) 
	// are unequal, asserts "item size error"
Array array_merge(Array* l, Array* r);
	
	// Splits input array (a) into two arrays: (r1) and (r2)
	// using the cut-by index.
	// Result items are written to provided Array pointers.
	//
	// If index goes outside of the capacity of the elements
	// assert "array buffer overflow" error
	//
	// Items are split by "before index :: index and after".
	// This means the item under the index and after will belong 
	// to second array, while all previous elements to first.
	//
	// if ARRAYFLAG_FREE_AFTER_CLONE is defined, frees input
	// array (a).
void array_split( Array* r1, Array* r2, Array* a, size_t index);

// COMMON DEFINITIONS FROM HCH

#ifndef INVALID_INDEX
#	define INVALID_INDEX (size_t)-1
#endif

#ifdef INVALID_INDEX
# if (INVALID_INDEX != ((size_t)-1))
#	error "INVALID_INDEX expected to be (size_t)-1, if other definition exists, edit the header source"
# endif
#endif


// if your platform is ... BAD and doesn't have
// 	bool
#ifndef __bool_true_false_are_defined
#	define bool		_Bool
#	define true 	1
#	define false 	0
#endif


#ifndef loopt
#	define loopt(TI,N) for(TI = 0; TI < (N); (TI)++)
#endif

#ifndef loop
#	define loop(I,N) for(size_t I = 0; I < (N); (I)++)
#endif

#ifndef ARRAY_INITIAL_CAPACITY
#	define ARRAY_INITIAL_CAPACITY 32
#endif

#define IGNORE_RETURN (void)


// IMPLEMENTATION
//#ifdef HCH_ARRAY_IMPLEMENTATION




Array __array_init(Array *a, 
		size_t capacity, 
		size_t elm_size) 
{
	assert(elm_size > 0 && 	"Invalid elm_size");
	Array temp	= {0}; 
	
	if (!a) {
		a = &temp;
	}

	a->count 	= 0;
	a->item_sz	= elm_size;
	a->capacity = (ARRAY_INITIAL_CAPACITY > capacity) ? 
		ARRAY_INITIAL_CAPACITY : capacity;

	a->data 	= ARRAY_MALLOC( a->capacity * elm_size 	);
	a->occupied = ARRAY_MALLOC( a->capacity 				);

	memset(a->data,0,	  a->capacity * elm_size 	);
	memset(a->data,0,	  a->capacity				); 

	return temp;
}

Array __array_from(
		void* arr, 
		size_t item_sz, 
		size_t length) 
{
	assert(arr && "Cannot create an array from a NULL pointer.");
	assert(item_sz	> 0 && 	"Invalid elm_size.");
	assert(length	> 0 && 	"Expected to have array of non-zero length for this function.");
	
	Array temp	= {0}; 
	__array_init(&temp, length*2, item_sz);

	memcpy(temp.data,arr,item_sz*length);
	loop(i,length) temp.occupied[i] = true;

	temp.count = length;

	return temp;
}

#define __array_mem_size(A) \
	(A).item_sz * (A).capacity

size_t __array_append_index(Array* a) {
	loop(i,a->count) if (a->occupied[i] == false) {
		return i;
	}
	return a->count;
}

void __array_insert(
		Array* a, 
		size_t index, 
		void* item, 
		size_t item_sz) {

	// on item greater size than biggest
	// numeric object, assert size 
	// being unequal due to attempt to 
	// insert inapropiate type into an array.
	assert(a->item_sz && "Attempt to insert into unitilized array");

	const size_t MAX_PLATFORM_SIZE = sizeof(size_t);
	if (a->item_sz > MAX_PLATFORM_SIZE)
		assert(item_sz == a->item_sz && "Mismatched type sizes");

	char* as_bytes 		= a->data;
	size_t mem_index	= index * a->item_sz;

	assert(index < a->capacity && "Array buffer overflow");
	memcpy(&(as_bytes[mem_index]),item,a->item_sz);
	a->occupied[index] = true;
	a->count = (a->count < index) ? index+1 : a->count+1;
}


void array_setflags(Array* a, array_bitmask mask) {
	a->flags = mask;
}

void array_clear(Array* a) {
	assert(a->data && "Expected to have initilized array");
	memset(a->data, 	0, a->capacity * a->item_sz);
	memset(a->occupied, 0, a->capacity             );
	a->count = 0;
}

void array_free(Array* a) {
	int flags = a->flags;
	if (a->flags & ARRAYFLAG_PEDANTIC_FREE) {
		assert(a->data && "Caught attempt to double free");
		assert(a->occupied && "Caught attempt to double free");
		ARRAY_FREE(a->data);
		ARRAY_FREE(a->occupied);
	} else {
		if (a->data)
			ARRAY_FREE(a->data);
		if (a->occupied)
			ARRAY_FREE(a->occupied);
	}
	if (a->scratch_buffer) 
		ARRAY_FREE(a->scratch_buffer);
	memset(a, 0, sizeof(*a));
	a->flags = flags;
}


void array_resize(Array* a, size_t new_size) {
	
	// save out precious time
	if (new_size == a->capacity)
		return;
	if (new_size == 0) {
		array_clear(a);
		return;
	}

	// TODO: ARRAYFLAG_AUTOFREE_CHILDREN
	if ( 	new_size < a->capacity &&
			a->flags & ARRAYFLAG_USE_DESTRUCTOR) 
	{
		for(size_t i = new_size; i < a->capacity; i++) {
			if (a->occupied[i]) {
				assert(a->destroy && "Attempt at calling destroy when its NULL");
				a->destroy(array_refer(*a,i));
		}}
	}

	size_t mem_new_size = a->item_sz * new_size;
	void* new = realloc(a->data, mem_new_size);
	assert(new && "realloc failed or got corrupted");
	a->data 	= new;
	a->capacity = new_size;
}



const void* array_raw(Array a) {
	assert(a.data && "Attmept to retrieve from NULL");
	return a.data;
}



size_t 		array_cap(Array a) {
	return a.capacity;
}



size_t 		array_len(Array a) {
	return a.count;
}



void* array_refer(Array a, size_t index) {
	assert(index < a.capacity && "Array buffer overflow");
	return (void*) (a.data + (a.item_sz * index));
}




void array_remove(Array* a, size_t index) {
	assert(index < a->capacity && "Array buffer overflow");
	size_t mem_index	= index * a->item_sz;
	void* item = a->data + mem_index;
	
	if (a->flags & ARRAYFLAG_USE_DESTRUCTOR) {
		assert(a->destroy && "Attempt at calling destroy when its NULL");
		a->destroy(item);
	}

	memset(item,0,a->item_sz);
	a->occupied[index] = false;
	a->count--;
}




bool array_is_valid(Array a) {
	return (a.data && a.occupied);
}


Array array_inherit(Array a) {
	Array follower = {0};
	follower.flags = a.flags;
	IGNORE_RETURN __array_init(
			&follower,
			a.capacity,
			a.item_sz);
	return follower;
}



Array array_dup(Array a) {
	Array clone = {0};
	IGNORE_RETURN __array_init(
			&clone,
			a.capacity,
			a.item_sz);

	memcpy(	clone.occupied,
			a.occupied,
			a.capacity);

	clone.count = a.count;
	clone.flags = a.flags;
	
	memcpy(	clone.data,a.data,
			__array_mem_size(a));
	return clone;
}



void array_swap(Array* a, size_t l, size_t r) {
	assert(a->capacity > l && a->capacity > r &&
			"Array buffer overflow");
	size_t l_mem_index	= l * a->item_sz;
	size_t r_mem_index	= r * a->item_sz;
	
	// its rare to us to want to 
	// swap elements in the array?
	// so use scratch element only when
	// needed.
	if (!a->scratch_buffer) {
		a->scratch_buffer = ARRAY_MALLOC(a->item_sz);
		memset(a->scratch_buffer,0,a->item_sz);
	}

	// left to scratch buffer
	memcpy(a->scratch_buffer, 
			a->data+l_mem_index, 
			a->item_sz);
	// right to where left was
	memcpy(a->data+l_mem_index, 
			a->data+r_mem_index, 
			a->item_sz);
	// scratch buffer to where right was
	memcpy(a->data+r_mem_index, 
			a->scratch_buffer, 
			a->item_sz);
}



void array_rebase(Array* a) {
	// rebasing is pretty rare, so for now
	// its good enough to do one array allocation 
	// considering its usecase purpouse
	Array shadow = array_inherit(*a);
	assert(shadow.count == 0 && "HMM, This has to be zero");

	loop(i,a->count) if (a->occupied[i]) {
		__array_insert(&shadow,
				shadow.count,
				a->data+(i*a->item_sz),
				a->item_sz
		);
	}
	array_free(a);
	*a = shadow;
}


void* array_pinch(Array* a, size_t index) {
	assert(index < a->capacity && "Array buffer overflow");
	

	if (index >= a->count) {
		return NULL;
	}

	void* item = array_refer(*a, index);
	void* copy = ARRAY_MALLOC(a->item_sz);
	memcpy(copy,item,a->item_sz);

	for(size_t i = index; i < a->count-1; i++) {
		array_swap(a,i,i+1);
	}
	a->count--;

	return copy;
}

Array array_merge(Array* l, Array* r) {
	Array merge = {0};
	assert(l->item_sz == r->item_sz &&
			"Merging arrays need to have equal item size");
	
	__array_init(
			&merge, 
			l->capacity + r->capacity, 
			l->item_sz
		);

	void* data_part1 = merge.data;
	void* data_part2 = merge.data + __array_mem_size(*l);

	void* occu_part1 = merge.occupied;
	void* occu_part2 = merge.occupied + l->capacity;

	// copy indexing of occupied items
	memcpy(occu_part1,l->occupied,l->capacity);
	memcpy(occu_part2,r->occupied,r->capacity);
	// copy owned data 
	memcpy(data_part1,l->data,__array_mem_size(*l));
	memcpy(data_part2,r->data,__array_mem_size(*r));

	merge.count = l->count + r->count;

	// free original if ordered to
	if (l->flags & ARRAYFLAG_FREE_AFTER_CLONE &&
		r->flags & ARRAYFLAG_FREE_AFTER_CLONE ) 
	{
		array_free(l);
		array_free(r);
	}
	
	return merge;
}

void array_split(
		Array* r1, Array* r2, 
		Array* a, 
		size_t index) 
{
	assert(
			!array_is_valid(*r1) &&
			!array_is_valid(*r2) &&
			"Input arrays have to be empty/uninitilized"
	);
	assert(a->capacity > index && "Array buffer overflow");

	__array_init(r1,a->capacity+a->count,a->item_sz);
	__array_init(r2,a->capacity+a->count,a->item_sz);

	void* src_part1 = a->data;
	void* src_part2 = a->data + a->item_sz*index; 

	void* src_occp1 = a->occupied;
	void* src_occp2 = a->occupied + index;

	memcpy(r1->data, src_part1, a->item_sz*index );
	memcpy(r2->data, src_part2, a->item_sz*(a->count-index));

	memcpy(r1->occupied, src_occp1, index );
	memcpy(r2->occupied, src_occp2, (a->count-index));

	r1->count = index;
	r2->count = a->count-index;

	if (a->flags & ARRAYFLAG_FREE_AFTER_CLONE) {
		array_free(a);
	}
}

// #endif // IMPLEMENTATION


// TODO: experiment with this :x{{{
#if 0 
size_t stack_sz() {
	struct rlimit limit;
	getrlimit (RLIMIT_STACK, &limit);
	return limit.rlim_cur;
}

int is_stack_var(void* ptr, size_t b, size_t e) {
	size_t addr = (size_t)ptr;
	if (addr > b && addr < e)
		return 1;
	return 0;
}
#endif/*}}}*/

#endif




#define ACT_AS_EXAMPLE


// EXAMPLE PROGRAM THAT 
// USES ARRAY HEADER:
#ifdef ACT_AS_EXAMPLE
int main(void) {
	// dummy data
	int n = 1;
	
	// Creates an array of default size 
	// for int type (or any other type that can be
	// fit in the place of int)
	Array arr = array_new(int);

	// I use shorthand `for` `loop`
	// you can `#undef loop` if you don't
	// like it.
	loop(i,16) {
		
		if (i == 0) 
			// appends to first empty
			// cell in the array,
			// if no gaps exist just puts in next 
			// and increments the count of items
			array_append(&arr,&n);
		else
			// insertion happens by reference, 
			// array_insert uses type size to check
			// if insertion is valid
			//
			// HOW INSERT AND APPEND ITERACT
			// TOGETHER, CHECK THE DOCS NEAR
			// THE DECLARATION!!!!
			array_insert(&arr,i,&n);
		n++;
	}

	// resized the array to given 
	// capacity
	array_resize(&arr, 16);

	// creates exact copy of the array with 
	// all flags 
	Array arr2 = array_dup(arr);
	
	//for(void* i = 0,  *it = array_refer(arr,(size_t)i);  (size_t)i < arr.count;  i++, it = array_refer(arr,(size_t)i%arr.count)) {  
	
	// swaps two elements at given indexes

	array_swap(&arr,1,10);

	int* i = array_pinch(&arr,15);

	if (i)
		printf("got: %i\n",*i);
	free(i);


	// acts like a style-preference
	// shorthand for writing loops
	// you can use it, or just ignore it
	//
	// since its a macro, u can #undef it
	// when you really really don't wanna see it
	array_foreach(arr, int* it,
		// creates a void* `it`
		// which, to use the iterator,
		// requires assigning to a "typed" pointer
		// like here:
		int *item = it;
		printf("%i, \n",*item);
	);

	printf("\n");

	// removes element from given index
	array_remove(&arr2, 2);
	array_remove(&arr2, 3);

	// rebases all elements in the array, so
	// that it has no gaps inbetween items
	array_rebase(&arr2);

	array_foreach(arr2, int* it,
		int *item = it;
		printf("%i, \n",*item);
	);


	array_remove_last(&arr2);


	// note that this is shorthand 
	// syntax for array_refer...
	//
	// and is only for convinience of 
	// not manually casting / dereferencing 
	// pointer..
	// so if something breaks, beacuse
	// you put wrong type or ignored return
	// value, then its on you.
	int last = array_last(arr2,int);

	printf("last: %i\n\n",last);


	
	// will create a copy of merged together
	// arrays, however we can oreder the 
	// consuption of input arrays by doing:
	
	//	array_setflags(&arr,	ARRAYFLAG_FREE_AFTER_CLONE);
	//	array_setflags(&arr2, 	ARRAYFLAG_FREE_AFTER_CLONE);

	array_setflags(&arr, ARRAYFLAG_PEDANTIC_FREE);
	Array merge = array_merge(&arr,&arr2);

	array_foreach(merge, int* it,
		int *item = it;
		printf("%i, \n",*item);
	);

	// we split the array in two by
	// using an index to chop from
	//
	// important to not that everything
	// at index and greates will be the part
	// of the second array.
	//
	// s1,s2 = {0}
	// arr = [ 1 2 3 4 ]
	// array_split(s1,s2,arr,1)
	//	   s1 == [ 1 ]
	//	   s2 == [ 2 3 4 ]
	Array s1 = {0}, s2 = {0};
	array_split(&s1,&s2, &merge, 25);

	printf("\n\nSplit one: \n");
	array_foreach(s1, int* it,
		int *item = it;
		printf("%i, \t",*item);
	);

	printf("\n\nSplit two: \n");
	array_foreach(s2, int* it,
		int *item = it;
		printf("%i, \t",*item);
	);


	printf("\nArray datatype from C array:\t");
	
	int array[] = {1,2,3,4,5,6,7};
	Array from = array_from(array,7);

	array_foreach(from, int* it,
			printf("{%i} ",*it);
	);

	printf("\nreverse order: ");

	array_eachfor(from, int* it,
			printf("{%i} ",*it);
	);

	// frees the entire array of memory, 
	// and clears all the flags
	//
	// to use again, call array_new or array_init
	array_free(&from);
	array_free(&s1);
	array_free(&s2);
	array_free(&arr);
	array_free(&arr2);
	array_free(&merge);
}
#endif // example main
