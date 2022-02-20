/* (c) Vladimir Dergachev 2019 */

#ifndef __LIBMVL_H__
#define __LIBMVL_H__

#include <stdio.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!  @file
 *   @brief core libMVL functions and structures
 */

/* Mappable Vector Library - 
 * a structured file format which can be efficiently used 
 * after read-only memory mapping, and can be appended while mapped, 
 * with versionable edits
 */

#define LIBMVL_SIGNATURE "MVL0"
#define LIBMVL_ENDIANNESS_FLAG 1.0

/*!
 *  @def LIBMVL_VECTOR_UINT8
 * 	MVL vector type for storing bytes and strings. Can also be used as an opaque type
 *  @def LIBMVL_VECTOR_INT32
 *      MVL vector type for storing 32-bit signed integers
 *  @def LIBMVL_VECTOR_INT64	
 *	  MVL vector type for storing 64-bit signed integers
 * @def LIBMVL_VECTOR_FLOAT	
 *	 MVL vector type for storing 32-bit floating point numbers
 * @def LIBMVL_VECTOR_DOUBLE	
 * 	MVL vector type for storing 64-bit floating point numbers
 * @def LIBMVL_VECTOR_OFFSET64	
 * 	MVL vector type for storing unsigned 64-bit offsets, typically considered as a list of other MVL vectors
 * @def LIBMVL_VECTOR_CSTRING	
 * 	MVL vector type for storing C-style strings. It is exactly as LIBMVL_VECTOR_UINT8, except that the data is considered valid up to length or first 0 byte 
 * @def LIBMVL_PACKED_LIST64 
*  The main purpose of this type is to provide efficient storage for vectors of short strings.
 * This is stored as LIBMVL_VECTOR_OFFSET64 with offset[0] pointing to the start of basic vector and subsequent offsets pointing to the start of the next string.
 * For convenience the last entry points to the end of the last string.
 * 
 * Thus the number of strings in PACKED_LIST64 is length-1.
 * 
 * The usage of 64-bit offsets allows for arbitrarily long strings in the list, while requiring only minimal overhead for each string.
 * 
 * The type is separate from LIBMVL_VECTOR_OFFSET64 to facilitate automated tree traversal.	
 */


#define LIBMVL_VECTOR_UINT8	1       
#define LIBMVL_VECTOR_INT32	2        
#define LIBMVL_VECTOR_INT64	3     
#define LIBMVL_VECTOR_FLOAT	4       
#define LIBMVL_VECTOR_DOUBLE	5       
#define LIBMVL_VECTOR_OFFSET64	100    
#define LIBMVL_VECTOR_CSTRING	101    

#define LIBMVL_PACKED_LIST64 	102     


#define LIBMVL_VECTOR_POSTAMBLE1 1000		/* Old format using DIRECTORY_ENTRY */
#define LIBMVL_VECTOR_POSTAMBLE2 1001		/* New format using named list */



/*! @brief Return the element size in bytes for a particular MVL type
 *  @param type MVL type, such LIBMVL_VECTOR_FLOAT
 *  @return size in bytes
 */
static inline int mvl_element_size(int type) 
{
switch(type) {
	case LIBMVL_VECTOR_UINT8:
	case LIBMVL_VECTOR_CSTRING:
		return 1;
	case LIBMVL_VECTOR_INT32:
	case LIBMVL_VECTOR_FLOAT:
		return 4;
	case LIBMVL_VECTOR_INT64:
	case LIBMVL_VECTOR_OFFSET64:
	case LIBMVL_VECTOR_DOUBLE:
	case LIBMVL_PACKED_LIST64:
		return 8;
	default:
		return(0);
	}
}


/*! @brief MVL unsigned 64-bit type used for describing offsets into loaded data
 */
typedef unsigned long long LIBMVL_OFFSET64;

/*! @brief This structure is written at the beginning of MVL file. It contains the signature identifying MVL format, and a means to check the endiannes of the MVL file 
 */
typedef struct {
	char signature[4];
	float endianness;
	unsigned int alignment;
	
	int reserved[13];
	} LIBMVL_PREAMBLE;

/*! @brief This structure is written last to close MVL file. It contains an offset to MVL directory that can be used to retrieve offsets to LIBMVL_VECTOR structures stored in MVL file
 */
typedef struct {
	LIBMVL_OFFSET64 directory;
	int type;
	int reserved[13];
	} LIBMVL_POSTAMBLE;

/*! @brief This structure describes the header of MVL vector. It is basically LIBMVL_VECTOR without the actual data
 */
typedef struct {
	LIBMVL_OFFSET64 length;
	int type;
	int reserved[11];
	LIBMVL_OFFSET64 metadata;
	} LIBMVL_VECTOR_HEADER;
	
#ifndef MVL_STATIC_MEMBERS
	
// #ifdef __SANITIZE_ADDRESS__
// #define MVL_STATIC_MEMBERS 0
// #warning "Address sanitizer active, using C11 definition of LIBMVL_VECTOR"
// #else
// #ifdef __clang__
// #if __has_feature(address_sanitizer)
// #define MVL_STATIC_MEMBERS 0
// #warning "Address sanitizer active, using C11 definition of LIBMVL_VECTOR"
// #else
// #define MVL_STATIC_MEMBERS 1
// #endif
// #else
// #define MVL_STATIC_MEMBERS 1
// #endif
// #endif	

#define MVL_STATIC_MEMBERS 1
#endif
	
#if MVL_STATIC_MEMBERS
/* This short and concise definition is portable and works with older compilers.
 * However, when the code is instrumented with an address sanitizer it chokes on it 
 * thinking that data arrays are smaller than they are.
 */
	
/*!  @brief LIBMVL_VECTOR is the basic unit of information storage
 */
typedef struct {
	LIBMVL_VECTOR_HEADER header;
	union {
		unsigned char b[8];
		int i[2];
		long long i64[1];
		float f[2];
		double d[1];
		LIBMVL_OFFSET64 offset[1];
		} u;
	} LIBMVL_VECTOR;
	
#else
/* This requires flexible array members and unnamed structs and unions which only appear in C11 standard 
 * The complexity arises because the standard does not allow flexible array members in a union which makes it cumbersome 
 * to describe variable size payloads.
 */
	
typedef union {
		struct {
		LIBMVL_VECTOR_HEADER header;
		unsigned char b[];
		};
		struct {
		LIBMVL_VECTOR_HEADER header1;
		int i[];
		};
		struct {
		LIBMVL_VECTOR_HEADER header2;
		long long i64[];
		};
		struct {
		LIBMVL_VECTOR_HEADER header3;
		float f[];
		};
		struct {
		LIBMVL_VECTOR_HEADER header4;
		double d[];
		};
		struct {
		LIBMVL_VECTOR_HEADER header5;
		LIBMVL_OFFSET64 offset[];
		};
	} LIBMVL_VECTOR;
#endif

	
/*! @brief This structure describes a named list - an array of LIBMVL_OFFSET64 entries each with a character name or tag.
 * 
 * A hash map can be computed for fast retrieval of entries by tag. 
 * It is allowed to have repeated names, but they are best avoided for compatibility with R.
 */
typedef struct {
	long size;
	long free;
	LIBMVL_OFFSET64 *offset;
	unsigned char **tag;
	long *tag_length;
	
	/* Optional hash table */
	
	long *next_item;
	long *first_item;
	LIBMVL_OFFSET64 hash_size;
	} LIBMVL_NAMED_LIST;
	
	
/*! @brief This structure describes MVL context - a collection of system data associated with a single MVL file. 
 * 
 *  For every accessed MVL file, whether for writing or via a memory map there must be one MVL context.
 */
typedef struct {
	int alignment;
	int error;

	LIBMVL_NAMED_LIST *directory;	
	LIBMVL_OFFSET64 directory_offset;

	LIBMVL_NAMED_LIST *cached_strings;
	
	LIBMVL_OFFSET64 character_class_offset;
	
	FILE *f;
	
	
	LIBMVL_PREAMBLE tmp_preamble;
	LIBMVL_POSTAMBLE tmp_postamble;
	LIBMVL_VECTOR_HEADER tmp_vh;
	
	int abort_on_error;
	int flags;
	
	} LIBMVL_CONTEXT;
	
#define LIBMVL_CTX_FLAG_HAVE_POSIX_FALLOCATE	 (1<<0)
#define LIBMVL_CTX_FLAG_HAVE_FTELLO	 	 (1<<1)
	
#define LIBMVL_ERR_FAIL_PREAMBLE	-1
#define LIBMVL_ERR_FAIL_POSTAMBLE	-2
#define LIBMVL_ERR_UNKNOWN_TYPE		-3
#define LIBMVL_ERR_FAIL_VECTOR		-4
#define LIBMVL_ERR_INCOMPLETE_WRITE	-5
#define LIBMVL_ERR_INVALID_SIGNATURE 	-6
#define	LIBMVL_ERR_WRONG_ENDIANNESS	-7
#define LIBMVL_ERR_EMPTY_DIRECTORY	-8
#define LIBMVL_ERR_INVALID_DIRECTORY	-9
#define LIBMVL_ERR_FTELL		-10
#define LIBMVL_ERR_CORRUPT_POSTAMBLE	-11
#define LIBMVL_ERR_INVALID_ATTR_LIST	-12
#define LIBMVL_ERR_INVALID_OFFSET	-13
#define LIBMVL_ERR_INVALID_ATTR		-14
#define LIBMVL_ERR_CANNOT_SEEK		-15
#define LIBMVL_ERR_INVALID_PARAMETER	-16
#define LIBMVL_ERR_INVALID_LENGTH	-17
#define LIBMVL_ERR_INVALID_EXTENT_INDEX	-18
	
LIBMVL_CONTEXT *mvl_create_context(void);
void mvl_free_context(LIBMVL_CONTEXT *ctx);

const char * mvl_strerror(LIBMVL_CONTEXT *ctx);

/*! @brief Use this constant to specify that no metadata should be written 
 */
#define LIBMVL_NO_METADATA 	0
/*! @brief Null offsets into memory mapped data are always invalid because that is where preamble is
 *  This is usually used to indicate that the offset does not point to valid data
 */
#define LIBMVL_NULL_OFFSET 	0


LIBMVL_OFFSET64 mvl_write_vector(LIBMVL_CONTEXT *ctx, int type, LIBMVL_OFFSET64 length, const void *data, LIBMVL_OFFSET64 metadata);

/* This is identical to mvl_write_vector() except that it allows to reserve space for more data than is supplied. */
LIBMVL_OFFSET64 mvl_start_write_vector(LIBMVL_CONTEXT *ctx, int type, LIBMVL_OFFSET64 expected_length, LIBMVL_OFFSET64 length, const void *data, LIBMVL_OFFSET64 metadata);
/* Rewrite data in already written vector with offset base_offset */
/* In particular this allows vectors to be built up in pieces, by calling mvl_start_write_vector first */
void mvl_rewrite_vector(LIBMVL_CONTEXT *ctx, int type, LIBMVL_OFFSET64 base_offset, LIBMVL_OFFSET64 idx, long length, const void *data);


LIBMVL_OFFSET64 mvl_write_concat_vectors(LIBMVL_CONTEXT *ctx, int type, long nvec, const long *lengths, void **data, LIBMVL_OFFSET64 metadata);

/* This computes vector vec[index] 
 * Indices do not have to be distinct
 * max_buffer is the maximum length of internal buffers in bytes (two buffers are needed for LIBMVL_PACKED_LIST64 vectors)
 */
LIBMVL_OFFSET64 mvl_indexed_copy_vector(LIBMVL_CONTEXT *ctx, LIBMVL_OFFSET64 index_count, const LIBMVL_OFFSET64 *indices, const LIBMVL_VECTOR *vec, const void *data, LIBMVL_OFFSET64 metadata, LIBMVL_OFFSET64 max_buffer);


/* Writes a single C string. In particular, this is handy for providing metadata tags */
/* length can be specified as -1 to be computed automatically */
LIBMVL_OFFSET64 mvl_write_string(LIBMVL_CONTEXT *ctx, long length, const char *data, LIBMVL_OFFSET64 metadata);

/* A cached version of the above that assures the string is only written once. No metadata because the strings are reused */
LIBMVL_OFFSET64 mvl_write_cached_string(LIBMVL_CONTEXT *ctx, long length, const char *data);

/* Create a packed list of strings 
 * str_size can be either NULL or provide string length, some of which can be -1 
 */
LIBMVL_OFFSET64 mvl_write_packed_list(LIBMVL_CONTEXT *ctx, long count, const long *str_size, unsigned char **str, LIBMVL_OFFSET64 metadata);

/* This is convenient for writing several values of the same type as vector without allocating a temporary array.
 * This function creates the array internally using alloca().
 */
LIBMVL_OFFSET64 mvl_write_vector_inline(LIBMVL_CONTEXT *ctx, int type, int count, LIBMVL_OFFSET64 metadata, ...);

#define MVL_NUMARGS(...)  (sizeof((int[]){__VA_ARGS__})/sizeof(int))


/*! \def MVL_WVEC 
 *   A convenience macro used for create and writing vectors of small number of entries inline. Commonly used for writing configuration data.
 *   
 *   Example: MVL_WVEC(ctx, LIBMVL_VECTOR_FLOAT, 1.0, 4.0, 9.0, 16.0)
 */
#define MVL_WVEC(ctx, type, ...) mvl_write_vector_inline(ctx, type, MVL_NUMARGS(__VA_ARGS__), 0, __VA_ARGS__)


void mvl_add_directory_entry(LIBMVL_CONTEXT *ctx, LIBMVL_OFFSET64 offset, const char *tag);
void mvl_add_directory_entry_n(LIBMVL_CONTEXT *ctx, LIBMVL_OFFSET64 offset, const char *tag, LIBMVL_OFFSET64 tag_size);
LIBMVL_OFFSET64 mvl_write_directory(LIBMVL_CONTEXT *ctx);

LIBMVL_NAMED_LIST *mvl_create_named_list(int size);
void mvl_free_named_list(LIBMVL_NAMED_LIST *L);

/* By default named lists are created by mvl_create_named_list() without a hash table, to make adding elements faster 
 * Calling this function creates the hash table. 
 * Note that functions reading lists from MVL files create hash table automatically.
 */
void mvl_recompute_named_list_hash(LIBMVL_NAMED_LIST *L);

long mvl_add_list_entry(LIBMVL_NAMED_LIST *L, long tag_length, const char *tag, LIBMVL_OFFSET64 offset);
LIBMVL_OFFSET64 mvl_find_list_entry(LIBMVL_NAMED_LIST *L, long tag_length, const char *tag);
LIBMVL_OFFSET64 mvl_write_attributes_list(LIBMVL_CONTEXT *ctx, LIBMVL_NAMED_LIST *L);

/* This is meant to operate on memory mapped (or in-memory) files */
LIBMVL_NAMED_LIST *mvl_read_attributes_list(LIBMVL_CONTEXT *ctx, const void *data, LIBMVL_OFFSET64 metadata_offset);

/* Convenience function that create a named list populated with necessary entries
 * It needs writable context to write attribute values */
LIBMVL_NAMED_LIST *mvl_create_R_attributes_list(LIBMVL_CONTEXT *ctx, const char *R_class);

/* Convenience function that returns an offset to attributes describing R-style character vector
 * The attributes are written out during the first call to this function
 */
LIBMVL_OFFSET64 mvl_get_character_class_offset(LIBMVL_CONTEXT *ctx);


/* This function writes contents of named list and creates R-compatible metadata with entry names */
LIBMVL_OFFSET64 mvl_write_named_list(LIBMVL_CONTEXT *ctx, LIBMVL_NAMED_LIST *L);
LIBMVL_OFFSET64 mvl_write_named_list2(LIBMVL_CONTEXT *ctx, LIBMVL_NAMED_LIST *L, char *cl);

/* This convenience function writes named list of vectors as R-compatible data frame. 
 * A well formatted data frame would have vectors of the same length specified as nrows
 * Assuring validity is up to the caller
 * 
 * rownames specifies an offset of optional row names of the data frame. Set as 0 to omit.
 */
LIBMVL_OFFSET64 mvl_write_named_list_as_data_frame(LIBMVL_CONTEXT *ctx, LIBMVL_NAMED_LIST *L, int nrows, LIBMVL_OFFSET64 rownames);

/* This is meant to operate on memory mapped (or in-memory) files */
LIBMVL_NAMED_LIST *mvl_read_named_list(LIBMVL_CONTEXT *ctx, const void *data, LIBMVL_OFFSET64 offset);

void mvl_open(LIBMVL_CONTEXT *ctx, FILE *f);
void mvl_close(LIBMVL_CONTEXT *ctx);
void mvl_write_preamble(LIBMVL_CONTEXT *ctx);
void mvl_write_postamble(LIBMVL_CONTEXT *ctx);

/*! @brief Return type of data from a pointer to LIBMVL_VECTOR 
 */
#define mvl_vector_type(data)   (((LIBMVL_VECTOR_HEADER *)(data))->type)

/*! @brief Return number of elements from a pointer to LIBMVL_VECTOR 
 */
#define mvl_vector_length(data)   (((LIBMVL_VECTOR_HEADER *)(data))->length)

#if MVL_STATIC_MEMBERS
/*! @brief Return base data from a pointer to LIBMVL_VECTOR 
 * 
 *  * Use mvl_vector_data(vec).b   for LIBMVL_VECTOR_UINT8
 *  * Use mvl_vector_data(vec).i   for LIBMVL_VECTOR_INT32
 *  * Use mvl_vector_data(vec).i64   for LIBMVL_VECTOR_INT64
 *  * Use mvl_vector_data(vec).f   for LIBMVL_VECTOR_FLOAT
 *  * Use mvl_vector_data(vec).d   for LIBMVL_VECTOR_DOUBLE
 *  * Use mvl_vector_data(vec).offsets   for LIBMVL_VECTOR_OFFSET64
 */
#define mvl_vector_data(data)   ((((LIBMVL_VECTOR *)(data))->u))
#else
#define mvl_vector_data(data)   (*(((LIBMVL_VECTOR *)(data))))
#endif


/*! 
 *  @def mvl_vector_data_uint8(data)
 * 	Access UINT8 array of LIBMVL_VECTOR
 * 	
 *  @def mvl_vector_data_int32(data)
 * 	Access INT32 array of LIBMVL_VECTOR
 * 	
 *  @def mvl_vector_data_int64(data)
 * 	Access INT64 array of LIBMVL_VECTOR
 * 	
 *  @def mvl_vector_data_float(data)
 * 	Access FLOAT array of LIBMVL_VECTOR
 * 	
 *  @def mvl_vector_data_double(data)
 * 	Access DOUBLE array of LIBMVL_VECTOR
 * 	
 *  @def mvl_vector_data_offset(data)
 * 	Access LIBMVL_OFFSET64 array of LIBMVL_VECTOR
 * 	
 * 
 * 
 */

#define mvl_vector_data_uint8(data)	((unsigned char *)(((const char *) data)+sizeof(LIBMVL_VECTOR_HEADER)))
#define mvl_vector_data_int32(data)	((int *)(((const char *) data)+sizeof(LIBMVL_VECTOR_HEADER)))
#define mvl_vector_data_int64(data)	((long long int *)(((const char *) data)+sizeof(LIBMVL_VECTOR_HEADER)))
#define mvl_vector_data_float(data)	((float *)(((const char *) data)+sizeof(LIBMVL_VECTOR_HEADER)))
#define mvl_vector_data_double(data)	((double *)(((const char *) data)+sizeof(LIBMVL_VECTOR_HEADER)))
#define mvl_vector_data_offset(data)	((LIBMVL_OFFSET64 *)(((const char *) data)+sizeof(LIBMVL_VECTOR_HEADER)))

/*! @brief Return offset to metadata of given LIBMVL_VECTOR
 */
#define mvl_vector_metadata_offset(data)   ((((LIBMVL_VECTOR_HEADER *)(data))->metadata))


/*! @brief This function returns 0 if the offset into data points to a valid vector, or a negative error code otherwise. 
 *  @param offset an offset into memory mapped data where the LIBMVL_VECTOR is located
 *  @param data  pointer to beginning of memory mapped data
 *  @param data_size an upper limit for valid offsets - usually the size of mapped MVL file.
 *  if data_size is set to ~0LLU the checks are bypassed
 */
static inline int mvl_validate_vector(LIBMVL_OFFSET64 offset, const void *data, LIBMVL_OFFSET64 data_size) {
LIBMVL_VECTOR *vec;
if(offset+sizeof(LIBMVL_VECTOR_HEADER)>data_size)return(LIBMVL_ERR_INVALID_OFFSET);
vec=(LIBMVL_VECTOR *)&(((unsigned char *)data)[offset]);

if(!mvl_element_size(mvl_vector_type(vec)))return LIBMVL_ERR_UNKNOWN_TYPE;

if(offset+sizeof(LIBMVL_VECTOR_HEADER)+mvl_vector_length(vec)>data_size)return(LIBMVL_ERR_INVALID_LENGTH);

if(mvl_vector_type(vec)==LIBMVL_PACKED_LIST64) {
	/* We check the first and last pointer of the packed list, as checking all the entries is inefficient
	 * A valid packed list will have all entries in increasing order, which is easy to check at the point of use
	 */
	LIBMVL_OFFSET64 offset2=mvl_vector_data_offset(vec)[0];
	LIBMVL_VECTOR *vec2;
	if(offset2 < sizeof(LIBMVL_VECTOR_HEADER) || offset2>data_size)return(LIBMVL_ERR_INVALID_OFFSET);

	vec2=(LIBMVL_VECTOR *)&(((unsigned char *)data)[offset2-sizeof(LIBMVL_VECTOR_HEADER)]);

	if(mvl_vector_type(vec2)!=LIBMVL_VECTOR_UINT8)return(LIBMVL_ERR_UNKNOWN_TYPE);
	if(offset2+mvl_vector_length(vec2)>data_size)return(LIBMVL_ERR_INVALID_LENGTH);
	
	if(mvl_vector_data_offset(vec)[mvl_vector_length(vec)-1]>offset2+mvl_vector_length(vec2))return(LIBMVL_ERR_INVALID_OFFSET);
	
	return(0);
	}

return(0);
}

/*! @brief A convenience function to convert an offset into memory mapped data into a pointer to LIBMVL_VECTOR structure.
 * 
 *  It assumes that the offset is valid, to validate it see mvl_validate_vector()
 * 
 *  @param data  pointer to memory mapped MVL file
 *  @param offset 64-bit offset into MVL file
 *  @return pointer to LIBMVL_VECTOR structure stored in MVL file
 */
static inline LIBMVL_VECTOR * mvl_vector_from_offset(void *data, LIBMVL_OFFSET64 offset)
{
return(offset==0 ? NULL : (LIBMVL_VECTOR *)(&(((unsigned char*)data)[offset])));
}



/* These two convenience functions are meant for retrieving a few values, such as stored configuration parameters.
 * Only floating point and offset values are supported as output because they have intrinsic notion of invalid value.
 */

/*! @brief Return idx vector entry as a double. 
 * 
 *  This function is meant as a convenience function for retrieving a few values, such as stored configuration parameters.
 * 
 * @param vec a pointer to LIBMVL_VECTOR
 * @param idx index into a vector
 * @return vector value converted into a double, or a NAN if anything went wrong.
 */

static inline double mvl_as_double(const LIBMVL_VECTOR *vec, long idx) 
{
if((idx<0) || (idx>=mvl_vector_length(vec)))return(NAN);

switch(mvl_vector_type(vec)) {
	case LIBMVL_VECTOR_DOUBLE:
		return(mvl_vector_data_double(vec)[idx]);
	case LIBMVL_VECTOR_FLOAT:
		return(mvl_vector_data_float(vec)[idx]);
	case LIBMVL_VECTOR_INT64:
		return(mvl_vector_data_int64(vec)[idx]);
	case LIBMVL_VECTOR_INT32:
		return(mvl_vector_data_int32(vec)[idx]);
	default:
		return(NAN);
	}
}

/*! @brief Return idx vector entry as a double, with default for missing values
 * 
 *  This function is meant as a convenience function for retrieving a few values, such as stored configuration parameters.
 * 
 * @param vec a pointer to LIBMVL_VECTOR
 * @param idx index into a vector
 * @param def default value to return in case of out of bounds indices.
 * @return vector value converted into a double, or def if anything went wrong.
 */
static inline double mvl_as_double_default(const LIBMVL_VECTOR *vec, long idx, double def) 
{
if((idx<0) || (idx>=mvl_vector_length(vec)))return(def);

switch(mvl_vector_type(vec)) {
	case LIBMVL_VECTOR_DOUBLE:
		return(mvl_vector_data_double(vec)[idx]);
	case LIBMVL_VECTOR_FLOAT:
		return(mvl_vector_data_float(vec)[idx]);
	case LIBMVL_VECTOR_INT64:
		return(mvl_vector_data_int64(vec)[idx]);
	case LIBMVL_VECTOR_INT32:
		return(mvl_vector_data_int32(vec)[idx]);
	default:
		return(def);
	}
}

/*! @brief Return idx vector entry as an offset. 
 * 
 *  This function is meant as a convenience function for retrieving a few values, such as stored configuration parameters.
 *  Only LIBMVL_VECTOR_OFFSET64 vectors are supported
 * 
 * @param vec a pointer to LIBMVL_VECTOR
 * @param idx index into a vector
 * @return vector value converted into a double, or LIBMVL_NULL_OFFSET if anything went wrong.
 */
static inline LIBMVL_OFFSET64 mvl_as_offset(const LIBMVL_VECTOR *vec, long idx) 
{
if((idx<0) || (idx>=mvl_vector_length(vec)))return(0);

switch(mvl_vector_type(vec)) {
	case LIBMVL_VECTOR_OFFSET64:
		return(mvl_vector_data_offset(vec)[idx]);
	default:
		return(0);
	}
}

/*! @brief Find an entry in a named list and return its idx value as a double. 
 * 
 *  This function is meant as a convenience function for retrieving a few values stored in a named list, such as stored configuration parameters.
 *  It effectively performs double indexing L[tag][idx]
 * 
 * @param L a pointer to previously retrieved LIBMVL_NAMED_LIST
 * @param data a pointer to beginning of memory mapped MVL file
 * @param tag_length length of character tag, or -1 to compute automatically
 * @param tag character tag
 * @param idx index into the entry
 * @return vector value converted into a double, or a NAN if anything went wrong.
 */
static inline double mvl_named_list_get_double(LIBMVL_NAMED_LIST *L, const void *data, long tag_length, const char *tag, long idx)
{
LIBMVL_VECTOR *vec;
LIBMVL_OFFSET64 ofs;
ofs=mvl_find_list_entry(L, tag_length, tag);
if(ofs==0)return(NAN);

vec=(LIBMVL_VECTOR *)&(((char *)data)[ofs]);
return(mvl_as_double(vec, idx));
}

/*! @brief Find an entry in a named list and return its idx value a double. 
 * 
 *  This function is meant as a convenience function for retrieving a few values stored in a named list, such as stored configuration parameters.
 *  It effectively performs double indexing L[tag][idx]
 * 
 * @param L a pointer to previously retrieved LIBMVL_NAMED_LIST
 * @param data a pointer to beginning of memory mapped MVL file
 * @param tag_length length of character tag, or -1 to compute automatically
 * @param tag character tag
 * @param idx index into the entry
 * @param def default value to return in case of errors
 * @return vector value converted into a double, or def if anything went wrong.
 */
static inline double mvl_named_list_get_double_default(LIBMVL_NAMED_LIST *L, const void *data, long tag_length, const char *tag, long idx, double def)
{
LIBMVL_VECTOR *vec;
LIBMVL_OFFSET64 ofs;
ofs=mvl_find_list_entry(L, tag_length, tag);
if(ofs==0)return(def);

vec=(LIBMVL_VECTOR *)&(((char *)data)[ofs]);
return(mvl_as_double_default(vec, idx, def));
}

/*! @brief Find an entry in a named list and return its idx value as an offset. 
 * 
 *  This function is meant as a convenience function for retrieving a few values stored in a named list, such as stored configuration parameters.
 *  It effectively performs double indexing L[tag][idx]
 * 
 * @param L a pointer to previously retrieved LIBMVL_NAMED_LIST
 * @param data a pointer to beginning of memory mapped MVL file
 * @param tag_length length of character tag, or -1 to compute automatically
 * @param tag character tag
 * @param idx index into the entry
 * @return vector value, or LIBMVL_NULL_OFFSET if anything went wrong.
 */
static inline LIBMVL_OFFSET64 mvl_named_list_get_offset(LIBMVL_NAMED_LIST *L, const void *data, long tag_length, const char *tag, long idx)
{
LIBMVL_VECTOR *vec;
LIBMVL_OFFSET64 ofs;
ofs=mvl_find_list_entry(L, tag_length, tag);
if(ofs==0)return(0);

vec=(LIBMVL_VECTOR *)&(((char *)data)[ofs]);
return(mvl_as_offset(vec, idx));
}

/*! @brief It is convenient to be able to mark strings as missing value, similar to NaN for floating point type. 
 *  In MVL this is done with the special string of length 4 consisting of two NUL characters followed by letters "NA"
 *
 */

#define MVL_NA_STRING "\000\000NA"
#define MVL_NA_STRING_LENGTH	4

static inline int mvl_string_is_na(const char *s, LIBMVL_OFFSET64 len)
{
if(len!=4)return 0;
if((s[0]==0 && s[1]==0 && s[2]=='N' && s[3]=='A'))return 1;
return(0);
}

/*! @brief Check whether packed list entry is a special string that indicates a missing value
 * @param vec a pointer to LIBMVL_VECTOR  with type LIBMVL_PACKED_LIST64
 * @param data a pointer to beginning of  memory mapped MVL file
 * @param idx entry index
 * @return 1 if the entry is NA - a missing value, 0 otherwise
 */
static inline int mvl_packed_list_is_na(const LIBMVL_VECTOR *vec, const void *data, LIBMVL_OFFSET64 idx)
{
LIBMVL_OFFSET64 start, stop, len;
if(mvl_vector_type(vec)!=LIBMVL_PACKED_LIST64)return 1;
len=mvl_vector_length(vec);
if((idx+1>=len) || (idx<0))return 1;
start=mvl_vector_data_offset(vec)[idx];
stop=mvl_vector_data_offset(vec)[idx+1];
return(mvl_string_is_na(&(((const char *)(data))[start]), stop-start));
}


/*! @brief Get length in bytes of string element idx from a packed list
 * @param vec a pointer to LIBMVL_VECTOR  with type LIBMVL_PACKED_LIST64
 * @param idx entry index
 * @return string length in bytes
 */
static inline LIBMVL_OFFSET64 mvl_packed_list_get_entry_bytelength(const LIBMVL_VECTOR *vec, LIBMVL_OFFSET64 idx)
{
LIBMVL_OFFSET64 start, stop, len;
if(mvl_vector_type(vec)!=LIBMVL_PACKED_LIST64)return -1;
len=mvl_vector_length(vec);
if((idx+1>=len) || (idx<0))return -1;
start=mvl_vector_data_offset(vec)[idx];
stop=mvl_vector_data_offset(vec)[idx+1];
return(stop-start);
}

/* This returns char even though the underlying type can be different - we just want the pointer */
/*! @brief Get pointer to the start of string element idx from a packed list
 * @param vec a pointer to LIBMVL_VECTOR with type LIBMVL_PACKED_LIST64
 * @param data a pointer to beginning of  memory mapped MVL file
 * @param idx entry index 
 * @return a pointer to the beginning of the data. 
 */
static inline const unsigned char * mvl_packed_list_get_entry(const LIBMVL_VECTOR *vec, const void *data, LIBMVL_OFFSET64 idx)
{
LIBMVL_OFFSET64 start, len;
if(mvl_vector_type(vec)!=LIBMVL_PACKED_LIST64)return NULL;
len=mvl_vector_length(vec);
if((idx+1>=len) || (idx<0))return NULL;
start=mvl_vector_data_offset(vec)[idx];
return(&(((const unsigned char *)(data))[start]));
}

LIBMVL_OFFSET64 mvl_find_directory_entry(LIBMVL_CONTEXT *ctx, const char *tag);

/* This initializes context to use in-memory image of given length starting at data
 * the image could have been loaded via fread, or memory mapped
 */
void mvl_load_image(LIBMVL_CONTEXT *ctx, LIBMVL_OFFSET64 length, const void *data);


/*! @def LIBMVL_SORT_LEXICOGRAPHIC
 *  Sort in ascending order
 *  @def LIBMVL_SORT_LEXICOGRAPHIC_DESC
 *  Sort in descending order
 */
#define LIBMVL_SORT_LEXICOGRAPHIC	1		/* Ascending */
#define LIBMVL_SORT_LEXICOGRAPHIC_DESC	2		/* Descending */

/*
 * This function sorts indices into a list of vectors so that the resulting permutation is ordered.
 * The vector should all be the same length N, except LIBMVL_PACKED_LIST64 which should N+1 - this provides the same number of elements.
 * The indices are from 0 to N-1 and can repeat.
 * 
 * vec_data is the pointer to mapped data range where offsets point. This is needed only for vectors of type LIBMVL_PACKED_LIST64.
 * You can set vec_data to NULL if LIBMVL_PACKED_LIST64 vectors are not present. Also entries vec_data[i] can be NULL if the corresponding vector is not of type
 * LIBMVL_PACKED_LIST64
 * 
 * This function return 0 on successful sort. If no vectors are supplies (vec_count==0) the indices are unchanged the sort is considered successful
 */
/*! @brief Given a table-like set of vectors of equal length arrange indices so that the columns are sorted lexicographically
 * 
 * @param indices_count  total number of indices 
 * @param indices an array of indices into provided vectors
 * @param vec_count the number of LIBMVL_VECTORS considered as columns in a table
 * @param vec an array of pointers to LIBMVL_VECTORS considered as columns in a table
 * @param vec_data an array of pointers to memory mapped areas those LIBMVL_VECTORs derive from. This allows computing hash from vectors drawn from different MVL files
 * @param sort_function one of LIBMVL_SORT_LEXICOGRAPHIC or LIBMVL_SORT_LEXICOGRAPHIC_DESC to specify sort direction
 */
int mvl_sort_indices(LIBMVL_OFFSET64 indices_count, LIBMVL_OFFSET64 *indices, LIBMVL_OFFSET64 vec_count, LIBMVL_VECTOR **vec, void **vec_data, int sort_function);


/* Hash function */

/* This randomizes bits of 64-bit numbers. */
/*! @brief Randomize bits of 64-bit numbers, typically after accumulating a hash value
 * 
 *  @param x input value
 *  @return Randomized value
 */
static inline LIBMVL_OFFSET64 mvl_randomize_bits64(LIBMVL_OFFSET64 x)
{
	x^=x>>31;
x*=18397683724573214587LLU;
	x^=x>>32;
x*=13397683724573242421LLU;
	x^=x>>33;
	return(x);
}

/* 32 bit primes: 2147483629 2147483647 */

/* Untested for randomization quality */
/*! @brief Randomize bits of 32-bit numbers, typically after accumulating a hash value
 *  @param x input value
 *  @return Randomized value
 */
static inline unsigned mvl_randomize_bits32(unsigned x)
{
	x^=x>>15;
x*=2354983627LLU;
	x^=x>>14;
x*=2554984639LLU;
	x^=x>>13;
	return(x);
}

/*! \def MVL_SEED_HASH_VALUE
 *   Recommended value to be used to initialize hashes. Note that initial value should not be 0
 */
#define MVL_SEED_HASH_VALUE	0xabcdef

/* This allows to accumulate hash value from several sources. 
 * Initial x value can be anything except 0 
 */
/*! @brief Accumulate hash from a piece of data. 
 * 
 *  This function allows to compute hash of data in several stages.
 *  @param x  previous hash value
 *  @param data array of character data
 *  @param count length of data
 *  @return new hash value
 */
static inline LIBMVL_OFFSET64 mvl_accumulate_hash64(LIBMVL_OFFSET64 x, const unsigned char *data, LIBMVL_OFFSET64 count)
{
LIBMVL_OFFSET64 i;
for(i=0;i<count;i++) {
	x=(x+data[i]);
	x*=13397683724573242421LLU;
	x^=x>>33;
	}
return(x);
}

/* This allows to accumulate hash value from several sources. 
 * Initial x values can be anything except 0.
 * The accumulation is done in place and in parallel for 8 streams, count bytes for each stream.
 */
static inline void mvl_accumulate_hash64x8(LIBMVL_OFFSET64 *x, const unsigned char *data0, const unsigned char *data1, const unsigned char *data2, const unsigned char *data3, const unsigned char *data4, const unsigned char *data5, const unsigned char *data6, const unsigned char *data7, LIBMVL_OFFSET64 count)
{
LIBMVL_OFFSET64 i, x0, x1, x2, x3, x4, x5, x6, x7;

x0=x[0];
x1=x[1];
x2=x[2];
x3=x[3];
x4=x[4];
x5=x[5];
x6=x[6];
x7=x[7];

for(i=0;i<count;i++) {
	#define STEP(k)  {\
		x ## k=( (x ## k) +(data ## k)[i]); \
		(x ## k)*=13397683724573242421LLU; \
		(x ## k) ^= (x ## k)>>33; \
		}
	STEP(0)
	STEP(1)
	STEP(2)
	STEP(3)
	STEP(4)
	STEP(5)
	STEP(6)
	STEP(7)
	#undef STEP
	}

x[0]=x0;
x[1]=x1;
x[2]=x2;
x[3]=x3;
x[4]=x4;
x[5]=x5;
x[6]=x6;
x[7]=x7;
}


/* This allows to accumulate hash value from several sources.
 * Initial x value can be anything except 0 
 * 
 * This function accumulates 32-bit signed ints by value
 */
/*! @brief Accumulate hash from an array of 32-bit integers
 *  The integers are hashed by value, not representation, so one gets the same hash from value of 100 whether it is stored as 32-bits or 64-bits.
 * 
 *  This function allows to compute hash of data in several stages.
 *  @param x  previous hash value
 *  @param data array of 32-bit integers
 *  @param count length of data
 *  @return new hash value
 */
static inline LIBMVL_OFFSET64 mvl_accumulate_int32_hash64(LIBMVL_OFFSET64 x, const int *data, LIBMVL_OFFSET64 count)
{
LIBMVL_OFFSET64 i;
long long int d;
unsigned *d_ext=(unsigned *)&d;
for(i=0;i<count;i++) {
	d=data[i];
	x=(x + d_ext[0]);
	x*=13397683724573242421LLU;
	x^=x>>33;
	x=(x + d_ext[1]);
	x*=13397683724573242421LLU;
	x^=x>>33;
	}
return(x);
}

/* This allows to accumulate hash value from several sources.
 * Initial x value can be anything except 0 
 * 
 * This function accumulates 64-bit signed ints by value
 */
/*! @brief Accumulate hash from an array of 64-bit integers
 *  The integers are hashed by value, not representation, so one gets the same hash from value of 100 whether it is stored as 32-bits or 64-bits.
 * 
 *  This function allows to compute hash of data in several stages.
 *  @param x  previous hash value
 *  @param data array of 64-bit integers
 *  @param count length of data
 *  @return new hash value
 */
static inline LIBMVL_OFFSET64 mvl_accumulate_int64_hash64(LIBMVL_OFFSET64 x, const long long int *data, LIBMVL_OFFSET64 count)
{
LIBMVL_OFFSET64 i;
long long int d;
unsigned *d_ext=(unsigned *)&d;
for(i=0;i<count;i++) {
	d=data[i];
	x=(x + d_ext[0]);
	x*=13397683724573242421LLU;
	x^=x>>33;
	x=(x + d_ext[1]);
	x*=13397683724573242421LLU;
	x^=x>>33;
	}
return(x);
}

/* This allows to accumulate hash value from several sources.
 * Initial x value can be anything except 0 
 * 
 * This function accumulates 32-bit floats by value, so that a float promoted to double will have the same hash
 */
/*! @brief Accumulate hash from an array of 32-bit floats
 *  The floats are hashed by value, not representation, so one gets the same hash from value of 100.0 whether it is stored as float or promoted to double.
 *  Note that this does not work in reverse - many doubles can be truncated to the same float.
 * 
 *  This function allows to compute hash of data in several stages.
 *  @param x  previous hash value
 *  @param data array of 32-bit floats
 *  @param count length of data
 *  @return new hash value
 */
static inline LIBMVL_OFFSET64 mvl_accumulate_float_hash64(LIBMVL_OFFSET64 x, const float *data, LIBMVL_OFFSET64 count)
{
LIBMVL_OFFSET64 i;
double d;
unsigned *d_ext=(unsigned *)&d;
for(i=0;i<count;i++) {
	d=data[i];
	x=(x + d_ext[0]);
	x*=13397683724573242421LLU;
	x^=x>>33;
	x=(x + d_ext[1]);
	x*=13397683724573242421LLU;
	x^=x>>33;
	}
return(x);
}

/* This allows to accumulate hash value from several sources.
 * Initial x value can be anything except 0 
 * 
 * This function accumulates 64-bit doubles by value, so that a float promoted to double will have the same hash as original float
 */
/*! @brief Accumulate hash from an array of 64-bit floats
 *  The floats are hashed by value, not representation, so one gets the same hash from value of 100.0 whether it is stored as float or promoted to double.
 *  Note that this does not work in reverse - many doubles can be truncated to the same float.
 * 
 *  This function allows to compute hash of data in several stages.
 *  @param x  previous hash value
 *  @param data array of 64-bit floats
 *  @param count length of data
 *  @return new hash value
 */
static inline LIBMVL_OFFSET64 mvl_accumulate_double_hash64(LIBMVL_OFFSET64 x, const double *data, LIBMVL_OFFSET64 count)
{
LIBMVL_OFFSET64 i;
double d;
unsigned *d_ext=(unsigned *)&d;
for(i=0;i<count;i++) {
	d=data[i];
	x=(x + d_ext[0]);
	x*=13397683724573242421LLU;
	x^=x>>33;
	x=(x + d_ext[1]);
	x*=13397683724573242421LLU;
	x^=x>>33;
	}
return(x);
}

/*! @brief Flags passed to mvl_hash_indices() and mvl_hash_range()
 * 
 *  Use LIBMVL_COMPLETE_HASH when computation is done in a single call, or spread out the computation over multiple calls.
 *  Initialization and finalization can also be done outside of mvl_hash_*() functions.
 * 
*   @def LIBMVL_ACCUMULATE_HASH
*     No initialization or finalization, just accumulate hash value
*   @def LIBMVL_INIT_HASH	
*     Initialize hash value, then accumulate
*   @def LIBMVL_FINALIZE_HASH	
*     Accumulate hash value and then finalize
*   @def LIBMVL_COMPLETE_HASH 
*    Initialize, accumulate, finalize.
*/


#define LIBMVL_ACCUMULATE_HASH	0
#define LIBMVL_INIT_HASH	1
#define LIBMVL_FINALIZE_HASH	2
#define LIBMVL_COMPLETE_HASH (LIBMVL_INIT_HASH | LIBMVL_FINALIZE_HASH)

/* This function is used to compute 64 bit hash of vector values
 * array hash[] is passed in and contains the result of the computation
 * 
 * Integer indices are computed by value, so that 100 produces the same hash whether it is stored as INT32 or INT64.
 * 
 * Floats and doubles are trickier - we can guarantee that the hash of float promoted to double is the same as the hash of the original float, but not the reverse.
 */
int mvl_hash_indices(LIBMVL_OFFSET64 indices_count, const LIBMVL_OFFSET64 *indices, LIBMVL_OFFSET64 *hash, LIBMVL_OFFSET64 vec_count, LIBMVL_VECTOR **vec, void **vec_data, int flags);
int mvl_hash_range(LIBMVL_OFFSET64 i0, LIBMVL_OFFSET64 i1, LIBMVL_OFFSET64 *hash, LIBMVL_OFFSET64 vec_count, LIBMVL_VECTOR **vec, void **vec_data, int flags);

/* This structure can either be allocated by libMVL or constructed by the caller 
 * In the latter case read comments describing size constraints 
 * 
 * The purpose of having index_size is to facilitate memory reuse by allocating the structure with index_size large enough to accomodate subsequent calls with different index_count
 */
/*! @brief Flags describing HASH_MAP state
 *  @def MVL_FLAG_OWN_HASH 
 *   HASH_MAP member hash owns allocated memory
 * @def MVL_FLAG_OWN_HASH_MAP
 *   HASH_MAP member hash_map owns allocated memory
 * @def MVL_FLAG_OWN_FIRST
 *   HASH_MAP member first owns allocated memory
 * @def MVL_FLAG_OWN_NEXT
 *   HASH_MAP member next owns allocated memory
 */
#define MVL_FLAG_OWN_HASH	(1<<0)
#define MVL_FLAG_OWN_HASH_MAP	(1<<1)
#define MVL_FLAG_OWN_FIRST	(1<<2)
#define MVL_FLAG_OWN_NEXT	(1<<3)
#define MVL_FLAG_OWN_VEC_TYPES	(1<<4)

/*! @brief This structure is used for constructing associative maps and also for describing index groupings
 * 
 *  This structure can either be allocated by mvl_allocate_hash_map() or constructed by the caller. In the latter case read comments describing size constraints. You can use flags to indicate that some arrays are not to be freed, such as when they point into memory mapped data.
 *  The purpose of having hash_size is to facilitate memory reuse by allocating the structure with hash_size large enough to accomodate subsequent calls with different hash_count
 */
typedef struct {
	LIBMVL_OFFSET64 flags; //!< flags describing HASH_MAP state
	LIBMVL_OFFSET64 hash_count; //!< Number of valid entries in hash, hash_count < hash_size and hash_count < hash_map_size
	LIBMVL_OFFSET64 hash_size; //!< size of hash, first and next arrays
	LIBMVL_OFFSET64 hash_map_size; //!<  size of hash_map array, should be power of 2
	LIBMVL_OFFSET64 first_count;  //!< Number of valid entries in first array - this is populated by mvl_find_groups()
	LIBMVL_OFFSET64 *hash;     //!<  Input hashes, used by mvl_compute_hash_map()
	LIBMVL_OFFSET64 *hash_map; //!<  This is an associative table mapping hash & (hash_map_size-1) into indices in the "first" array
	LIBMVL_OFFSET64 *first;  //!< array of indices in each group
	LIBMVL_OFFSET64 *next; //!< array of next indices in each group. ~0LLU indicates end of group
	LIBMVL_OFFSET64 vec_count;  //!< Number of vectors used to produce hashes
	int *vec_types; //!< Types of vectors used to produce hashes
	} HASH_MAP;

/* Compute suggested hash map size */
LIBMVL_OFFSET64 mvl_compute_hash_map_size(LIBMVL_OFFSET64 hash_count);

HASH_MAP *mvl_allocate_hash_map(LIBMVL_OFFSET64 max_index_count);
void mvl_free_hash_map(HASH_MAP *hash_map);

/* This uses data from hm->hash[] array */
void mvl_compute_hash_map(HASH_MAP *hm);

/* Find count of matches between hashes of two sets. 
 */
LIBMVL_OFFSET64 mvl_hash_match_count(LIBMVL_OFFSET64 key_count, const LIBMVL_OFFSET64 *key_hash, HASH_MAP *hm);

/* Find indices of keys in set of hashes, using hash map. 
 * Only the first matching hash is reported.
 * If not found the index is set to ~0 (0xfff...fff)
 * Output is in key_indices 
 */
void mvl_find_first_hashes(LIBMVL_OFFSET64 key_count, const LIBMVL_OFFSET64 *key_hash, LIBMVL_OFFSET64 *key_indices, HASH_MAP *hm);

/* This function computes pairs of merge indices. The pairs are stored in key_match_indices[] and match_indices[].
 * All arrays should be provided by the caller. The size of match_indices arrays is computed with mvl_hash_match_count()
 * An auxiliary array key_last of length key_indices_count stores the stop before index (in terms of matches array). 
 * In particular the total number of matches is given by key_last[key_indices_count-1]
 */
int mvl_find_matches(LIBMVL_OFFSET64 key_indices_count, const LIBMVL_OFFSET64 *key_indices, LIBMVL_OFFSET64 key_vec_count, LIBMVL_VECTOR **key_vec, void **key_vec_data, LIBMVL_OFFSET64 *key_hash,
			   LIBMVL_OFFSET64 indices_count, const LIBMVL_OFFSET64 *indices, LIBMVL_OFFSET64 vec_count, LIBMVL_VECTOR **vec, void **vec_data, HASH_MAP *hm, 
			   LIBMVL_OFFSET64 *key_last, LIBMVL_OFFSET64 pairs_size, LIBMVL_OFFSET64 *key_match_indices, LIBMVL_OFFSET64 *match_indices);

/* This function transforms HASH_MAP into a list of groups. 
 * After calling hm->hash_map is invalid, but hm->first and hm->next describe exactly identical rows 
 */
void mvl_find_groups(LIBMVL_OFFSET64 indices_count, const LIBMVL_OFFSET64 *indices, LIBMVL_OFFSET64 vec_count, LIBMVL_VECTOR **vec, void **vec_data, HASH_MAP *hm);


/*! @brief List of offsets partitioning the vector. First element is always 0, last element is vector size.
 * 
 *  These are very convenient to describe vector stretches with some properties. Also see description of LIBMVL_PACKED_LIST64.
 */
typedef struct {
	LIBMVL_OFFSET64 size;   //!<  Space allocated for start and stop arrays
	LIBMVL_OFFSET64 count;  //!<  extent has count valid elements
	LIBMVL_OFFSET64 *offset; //!<  First extent element
	} LIBMVL_PARTITION;

void mvl_init_partition(LIBMVL_PARTITION *el);
void mvl_extend_partition(LIBMVL_PARTITION *el, LIBMVL_OFFSET64 nelem);
void mvl_find_repeats(LIBMVL_PARTITION *partition, LIBMVL_OFFSET64 count, LIBMVL_VECTOR **vec, void **data);
void mvl_free_partition_arrays(LIBMVL_PARTITION *el);

#ifndef LIBMVL_EXTENT_INLINE_SIZE
#define LIBMVL_EXTENT_INLINE_SIZE 4
#endif

/*! @brief List of extents - ranges of consequentive indices. Similar to partition, but they do not have to follow each other.
 * 
 *  The inline arrays are there to optimize for the common case of few extents and reduce the number of memory allocation calls.
 */
typedef struct {
	LIBMVL_OFFSET64 size; //!<  Space allocated for start and stop arrays
	LIBMVL_OFFSET64 count; //!<  extent has count valid elements
	LIBMVL_OFFSET64 *start; //!<  First extent element
	LIBMVL_OFFSET64 *stop; //!<  First element just past the extent end
	LIBMVL_OFFSET64 start_inline[LIBMVL_EXTENT_INLINE_SIZE];
	LIBMVL_OFFSET64 stop_inline[LIBMVL_EXTENT_INLINE_SIZE];
	} LIBMVL_EXTENT_LIST;

void mvl_init_extent_list(LIBMVL_EXTENT_LIST *el);
void mvl_free_extent_list_arrays(LIBMVL_EXTENT_LIST *el);
void mvl_extend_extent_list(LIBMVL_EXTENT_LIST *el, LIBMVL_OFFSET64 nelem);

/*! @brief An index into a table-like set of vectors with equal number of elements
 * 
 *  While it would work for any such vector set the structure has been optimized for the case of rows with repeated values,
 *  such as occur with sorted tables.
 */
typedef struct {
	LIBMVL_PARTITION partition;
	HASH_MAP hash_map;
	} LIBMVL_EXTENT_INDEX;

	
void mvl_init_extent_index(LIBMVL_EXTENT_INDEX *ei);
void mvl_free_extent_index_arrays(LIBMVL_EXTENT_INDEX *ei);
int mvl_compute_extent_index(LIBMVL_EXTENT_INDEX *ei, LIBMVL_OFFSET64 count, LIBMVL_VECTOR **vec, void **data);
LIBMVL_OFFSET64 mvl_write_extent_index(LIBMVL_CONTEXT *ctx, LIBMVL_EXTENT_INDEX *ei);
int mvl_load_extent_index(LIBMVL_CONTEXT *ctx, void *data, LIBMVL_OFFSET64 offset, LIBMVL_EXTENT_INDEX *ei);

/*! @brief Alter extent list to contain no extents without freeing memory
 * 
 *  @param el pointer to extent list structure to empty
 */
static inline void mvl_empty_extent_list(LIBMVL_EXTENT_LIST *el)
{
el->count=0;
}


/*! @brief Find extents in index corresponding to a given hash
 * 
 *  @param ei pointer to populated extent index structure
 *  @param hash 64-bit hash value to query
 *  @param el pointer to extent list structure to add extents to
 */
static inline void mvl_get_extents(LIBMVL_EXTENT_INDEX *ei, LIBMVL_OFFSET64 hash, LIBMVL_EXTENT_LIST *el)
{
LIBMVL_OFFSET64 idx, count;

count=ei->hash_map.hash_count;
idx=ei->hash_map.hash_map[hash & (ei->hash_map.hash_map_size-1)];

while(idx<count) {
	if(hash==ei->hash_map.hash[idx]) {
		if(el->count>=el->size)mvl_extend_extent_list(el, 0);
		el->start[el->count]=ei->partition.offset[idx];
		el->stop[el->count]=ei->partition.offset[idx+1];
		el->count++;
		}
	idx=ei->hash_map.next[idx];
	}
}


/*! @brief Vector statistics.
 * 
 *  This structure can be allocated on stack.
 */
typedef struct {
	double max; //!< maximum value of vector entries
	double min; //!< minimum value of vector entries
	double center; //!< a value in the "middle" of the vector
	double scale;  //!< normalization scale
	double average_repeat_length; //!< average length of stretch with identical elements
	double nrepeat; //!< number of stretches with identical elements
	} LIBMVL_VEC_STATS;

void mvl_compute_vec_stats(const LIBMVL_VECTOR *vec, LIBMVL_VEC_STATS *stats);
/* i0 and i1 denote the range of values to normalize. This allows to process vector one buffer at a time */
void mvl_normalize_vector(const LIBMVL_VECTOR *vec, const LIBMVL_VEC_STATS *stats, LIBMVL_OFFSET64 i0, LIBMVL_OFFSET64 i1, double *out);

/*! @brief Index types
 * 
 */
#define MVL_EXTENT_INDEX	1
#define MVL_SPATIAL_INDEX1	2


#ifdef __cplusplus
}
#endif

#endif
