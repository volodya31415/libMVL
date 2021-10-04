libMVL - Mappable vector library, use all of your data at once, with few limits
===============================================================================

Mappable vector library - MVL is a data format designed for ease of use when memory mapped. 

This was achieved by basing the format around the notion of vectors - linear arrays of primitive data types, such as char, integer, float or offset.
The arrays are written so that they are aligned to the start of the file (default alignment is 64 bytes). This facilitates vector operations.

An MVL file consists of a preamble, followed by a number of MVL vectors and then a postamble that closes the file. 

   <Preamble> [ <MVL vector> ... <MVL vector> ] <Postamble>
   
Preamble starts with sequence MVL0 that identifies the file format, and has a means to identify endianness of the underlying data
and alignment used to write the file.

The postamble contains a pointer to a directory that allows to associate offsets to individual vectors with symbolic tags.

Despite simplicity of layout one can store data of arbitrary complexity by using MVL vectors of offset type that act as lists.

In addition, any of the basic integer types can be used as an opaque type to store arbitrary data, such as compressed streams or arrays of structures.

The format is fully 64-bit, and MVL vectors can have lengths of up to 2^62 - essentially only limited by the amount of storage available.

A few examples:

* data stored in MVL file can be memory mapped into R interpreter using package "RMVL". This permits working with data sets far exceeding memory capacity of the host.
      
* data stored in MVL file can be memory mapped by individual processes running on the cluster. Because the memory is shared this reduces overall memory requirements.
        Moreover, data is loaded from NFS servers only once to each node greatly decreasing cluster I/O.
        
* A C program under development can write its internal data to MVL file using symbolic tags that describe which variables held the data. After tests, the MVL files can be loaded into R for analysis.
      
      
As mentioned above there is a package "RMVL" for R that allows to read and create MVL files. It has its own documentation
https://CRAN.R-project.org/package=RMVL

C interface to libMVL
---------------------

Each MVL file whether read, written or memory mapped needs its own LIBMVL_CONTEXT:

> LIBMVL_CONTEXT *ctx=mvl_create_context();

The context is destroyed using mvl_free_context(ctx);

In order to write to the MVL file the user needs to provide stdio FILE pointer using mvl_open():

> FILE *fout=fopen("test.mvl", "w");
>
> mvl_open(ctx, fout);


When the user is finished writing to MVL file it needs to be closed with:

> mvl_close(ctx);

before the context is destroyed.

Individual vectors can be written with mvl_write_vector():

> LIBMVL_OFFSET64 ofs=mvl_write_vector(ctx, TYPE, LENGTH, DATA, METADATA);

The TYPE can be any of elementary types described in libMVL.h. LENGTH describes the number of elements
of given type in DATA, which is a pointer an array. Each element of DATA has size that can be retrieved with mvl_element_size(TYPE).

METADATA is an offset into the file pointing to one of the vectors. It is optional - provide 0 or LIBMVL_NO_METADATA if you don't need it.

The METADATA can be used to store additional information using by external programs like R, as well as dimensions of multidimensional arrays.

The function mvl_write_vector() is convenient for writing fully formed arrays. Take a look at functions mvl_write_concat_vectors(), mvl_indexed_copy_vector(), mvl_write_string() and others in libMVL.c file for more ways to write out data.

As you write MVL vectors you obtain offsets into the MVL file where these vectors are written. 
These offsets can be stored in an offset array (LIBMVL_VECTOR_OFFSET64) to be written as LIBMVL_VECTOR_OFFSET64, or they can 
be recorded in the directory with:

> mvl_add_directory_entry(ctx, ofs, TAG);

The TAG memory is a character string that allows to retrieve the offset later when the file is opened. The TAGs do not have to be unique, but this is recommended.
The directory is scanned backwards so that the TAG written last is retrieved first. 

The directory is written out by mvl_close(ctx), this call is essential to produce well-formed MVL file.

To access previously written MVL file the user loads the data by reading it or memory mapping the file.
Then the program should call

> char * MAPPED_FILE;
> mvl_load_image(ctx, LENGTH, MAPPED_FILE)

where MAPPED_FILE is the pointer to the loaded file, and LENGTH is the length of file. It is important to get the LENGTH right, so that the postamble can be accessed to load the directory.

Once the image is loaded, the directory can be accessed with

> ofs=mvl_find_directory_entry(ctx, TAG)

Assuming MAPPED_FILE is a pointer to char, the vectors can be accessed as

> LIBMVL_VECTOR *vec=(LIBMVL_VECTOR *) &(MAPPED_FILE[ofs]);

Some convenience functions for accessing vector data:

* mvl_vector_type(vec) returns type

* mvl_vector_length(vec) returns number of elements

* mvl_vector_data(vec) returns the pointer to the start of the array:
   + mvl_vector_data(vec).b for  char
   + mvl_vector_data(vec).i for  integer
   + mvl_vector_data(vec).i64 for  64 bit integers
   + mvl_vector_data(vec).f for  float
   + mvl_vector_data(vec).d for  double
   + mvl_vector_data(vec).offset for  LIBMVL_OFFSET64 (unsigned 64-bit offset)

* mvl_vector_metadata_offset(vec) returns offset to metadata for this vector
	
A common sitation is a need to store or retrieved a named list that associates offsets with symbolic names.
This is facilitated with LIBMVL_NAMED_LIST structure.

One creates it with 

> LIBMVL_NAMED_LIST *L=mvl_create_named_list(SIZE);

SIZE is the expected size of the list, but it will grow as needed.

The structure is freed with mvl_free_named_list(L). Note that context is not required - this is internal data only.

Elements are added to the list with mvl_add_list_entry() and retrieved with mvl_find_list_entry().

The list can be written to MVL file via: 

> ofs=mvl_write_named_list(ctx, L);

It can be read from loaded data with 

> L=mvl_read_named_list(ctx, MAPPED_FILE, ofs);

To accomodate named metadata attributes, it is stored as named list:

> moffset=mvl_write_attributes_list(ctx, L);
>
> L=mvl_read_attributes_list(ctx, MAPPED_FILE, moffset);

There are convenience function that create metadata easily interpretable by R:

> L=mvl_create_R_attributes_list(ctx, RCLASS);

RCLASS is character string giving a name of one of R classes.

> moffset=mvl_write_named_list_as_data_frame(ctx, L, nrows, LIBMVL_OFFSET64 rownames_offset);

This creates metadata for R-style data frame - a list of equal length vectors. This is equivalent to a table in a database.
The argument rownames_offset is optional - if it is 0, rownames would be created upon loading to R.

Sometimes one needs to create very short vectors with just a few members. For example, the "dim" metadata attribute gives array dimensions and 
usually has just a few entries. This can be done conveniently with macro MVL_WVEC() that allows inline writing of vectors:

> ofs=MVL_WVEC(ctx, TYPE, ...);

Here TYPE is any of the elementary types (such as LIBMVL_VECTOR_INT64 for 64 bit integers), and ... are the elements of the MVL vector.

The character vectors can be stored using LIBMVL_VECTOR_UINT8 or LIBMV_VECTOR_CSTRING. The latter expects to include terminating 0. This can be inefficient if many short strings need to be stored.

Often one only needs to write the same string repeatedly - for this there is a function mvl_write_cached_string() that writes C-style strings. The string is only written once during the first call, and the the offset is reused.

To store vectors of strings of arbirary length efficiently one can use the following helper functions:

> ofs=mvl_write_packed_list(ctx, COUNT, STR_SIZE, STRINGS, METADATA);
>
> size=mvl_packed_list_get_entry_bytelength(VEC, IDX)
>
> const char * str=mvl_packed_list_get_entry(VEC, MAPPED_FILE, IDX)

Here COUNT is the number of strings to be written out (i.e. the length of string array), STR_SIZE is the array of individual string lengths. STR_SIZE could be NULL in which case string lengths are computed automatically. If any STR_SIZE element is negative then the length of corresponding string is computed automatically. STRINGS is an array of char * pointers.

mvl_write_packed_list() produces a MVL vector of type LIBMVL_PACKED_LIST64 which number of elements is one more than the length of the string array written. 
The elements can be retrieved using mvl_packed_list_get_entry(VEC, IDX) which returns the length of the stored string with index IDX and mvl_packed_list_get_entry(VEC, MAPPED_FILE, IDX) which returns the pointer to string IDX.

Database functions
------------------

A common situation is when the data is naturally organized in records of fixed number of fields and one needs to select groups of records with particular values or
match records on particular fields. This is typically accomplished with a database software, such as MariaDB, sqlite or postgresql. 

An MVL file can act as a database that an application has a direct access to. Since the access is unrestricted any set of vectors with the same number of elements can be considered a "table". However, it is often convenient to organize them into a LIBMVL_NAMED_LIST which tags are interpreted as column names.

libMVL has a number of functions that make it easier to use and manage tables:

* mvl_sort_indices() obtains permutations that sort rows

* mvl_hash_indices() obtains 64-bit hash that identify rows for further processing. The hashes have good statistical properties and can be truncated to any number of bits. The hashing is done by value, so 100 will produce the same hash when stored as either 32 or 64 bit integer.

* mvl_compute_hash_map() computes an associative array that maps hashes to corresponding groups of rows. Also see mvl_allocate_hash_map(), 
mvl_compute_hash_map_size(), mvl_free_hash_map()

* mvl_find_matches() finds exact matches between two sets of rows using a previously computed hash_map. The matches are first identified using hashes and then
rows are compared for equality. The comparison is done by value, so it does not matter whether 100 is stored as 32 or 64 bits integer. Also see mvl_hash_match_count() which provides estimates of number of matches useful for allocating arrays.

* mvl_find_groups() transforms a previously computed hash map into a set of groups of identical rows.

* mvl_normalize_vector() computes statistics useful for construction of spatial indices



