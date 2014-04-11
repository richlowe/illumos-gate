provider ld {
	probe malloc__success(size_t); /* Successful allocation */
	probe malloc__failure(size_t); /* Failed allocation */
	probe heap__grow(size_t);      /* Heap growth */
};

#pragma D attributes Private/Private/ISA provider ld provider
#pragma D attributes Private/Private/Unknown provider ld module
#pragma D attributes Private/Private/Unknown provider ld function
#pragma D attributes Private/Private/ISA provider ld name
#pragma D attributes Private/Private/ISA provider ld args
