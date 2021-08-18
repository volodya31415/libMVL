/* (c) Vladimir Dergachev 2019 */

#ifndef __LIBMVL_H__
#define __LIBMVL_H__

#include <stdio.h>
#include <math.h>

/* Mappable Vector Library - 
 * a structured file format which can be efficiently used 
 * after read-only memory mapping, and can be appended while mapped, 
 * with versionable edits
 */

#define LIBMVL_SIGNATURE "MVL0"
#define LIBMVL_ENDIANNESS_FLAG 1.0

#define LIBMVL_VECTOR_UINT8	1
#define LIBMVL_VECTOR_INT32	2
#define LIBMVL_VECTOR_INT64	3
#define LIBMVL_VECTOR_FLOAT	4
#define LIBMVL_VECTOR_DOUBLE	5
#define LIBMVL_VECTOR_OFFSET64	100
#define LIBMVL_VECTOR_CSTRING	101     /* C string is just like UINT8, except that the data is considered valid up to length or
first 0 byte */

/* The main purpose of this type is to provide efficient storage for vectors of short strings.
 * This is stored as LIBMVL_VECTOR_OFFSET64 with offset[0] pointing to the start of basic vector and subsequent offsets pointing to the start of the next string.
 * For convenience the last entry points to the end of the last string.
 * 
 * Thus the number of strings in PACKED_LIST64 is length-1.
 * 
 * The usage of 64-bit offsets allows for arbitrarily long strings in the list, while requiring only minimal overhead for each string.
 * 
 * The type is separate from LIBMVL_VECTOR_OFFSET64 to facilitate automated tree traversal.
 */
#define LIBMVL_PACKED_LIST64 	102     

#define LIBMVL_VECTOR_POSTAMBLE 1000

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


typedef unsigned long long LIBMVL_OFFSET64;

typedef struct {
	char signature[4];
	float endianness;
	unsigned int alignment;
	
	int reserved[13];
	} LIBMVL_PREAMBLE;

typedef struct {
	LIBMVL_OFFSET64 directory;
	int type;
	int reserved[13];
	} LIBMVL_POSTAMBLE;
	
typedef struct {
	LIBMVL_OFFSET64 length;
	int type;
	int reserved[11];
	LIBMVL_OFFSET64 metadata;
	} LIBMVL_VECTOR_HEADER;
	
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

typedef struct {
	LIBMVL_OFFSET64 offset;
	char *tag;
	} LIBMVL_DIRECTORY_ENTRY;
	
typedef struct {
	long size;
	long free;
	LIBMVL_OFFSET64 *offset;
	char **tag;
	long *tag_length;
	
	/* Optional hash table */
	
	long *next_item;
	long *first_item;
	long hash_size;
	long hash_mult;
	} LIBMVL_NAMED_LIST;
	
typedef struct {
	int alignment;
	int error;

	long dir_size;
	long dir_free;
	LIBMVL_DIRECTORY_ENTRY *directory;	
	LIBMVL_OFFSET64 directory_offset;

	LIBMVL_NAMED_LIST *cached_strings;
	
	LIBMVL_OFFSET64 character_class_offset;
	
	FILE *f;
	
	
	LIBMVL_PREAMBLE tmp_preamble;
	LIBMVL_POSTAMBLE tmp_postamble;
	LIBMVL_VECTOR_HEADER tmp_vh;
	
	int abort_on_error;
	
	} LIBMVL_CONTEXT;
	
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
	
LIBMVL_CONTEXT *mvl_create_context(void);
void mvl_free_context(LIBMVL_CONTEXT *ctx);

#define LIBMVL_NO_METADATA 	0
#define LIBMVL_NULL_OFFSET 	0

LIBMVL_OFFSET64 mvl_write_vector(LIBMVL_CONTEXT *ctx, int type, long length, const void *data, LIBMVL_OFFSET64 metadata);

/* This is identical to mvl_write_vector() except that it allows to reserve space for more data than is supplied. */
LIBMVL_OFFSET64 mvl_start_write_vector(LIBMVL_CONTEXT *ctx, int type, long expected_length, long length, const void *data, LIBMVL_OFFSET64 metadata);
/* Rewrite data in already written vector with offset base_offset */
/* In particular this allows vectors to be built up in pieces, by calling mvl_start_write_vector first */
void mvl_rewrite_vector(LIBMVL_CONTEXT *ctx, int type, LIBMVL_OFFSET64 base_offset, LIBMVL_OFFSET64 idx, long length, const void *data);


LIBMVL_OFFSET64 mvl_write_concat_vectors(LIBMVL_CONTEXT *ctx, int type, long nvec, long *lengths, void **data, LIBMVL_OFFSET64 metadata);
/* Writes a single C string. In particular, this is handy for providing metadata tags */
/* length can be specified as -1 to be computed automatically */
LIBMVL_OFFSET64 mvl_write_string(LIBMVL_CONTEXT *ctx, long length, const char *data, LIBMVL_OFFSET64 metadata);

/* A cached version of the above that assures the string is only written once. No metadata because the strings are reused */
LIBMVL_OFFSET64 mvl_write_cached_string(LIBMVL_CONTEXT *ctx, long length, const char *data);

/* Create a packed list of strings 
 * str_size can be either NULL or provide string length, some of which can be -1 
 */
LIBMVL_OFFSET64 mvl_write_packed_list(LIBMVL_CONTEXT *ctx, long count, long *str_size, char **str, LIBMVL_OFFSET64 metadata);

/* This is convenient for writing several values of the same type as vector without allocating a temporary array.
 * This function creates the array internally using alloca().
 */
LIBMVL_OFFSET64 mvl_write_vector_inline(LIBMVL_CONTEXT *ctx, int type, int count, LIBMVL_OFFSET64 metadata, ...);

#define MVL_NUMARGS(...)  (sizeof((int[]){__VA_ARGS__})/sizeof(int))

#define MVL_WVEC(ctx, type, ...) mvl_write_vector_inline(ctx, type, MVL_NUMARGS(__VA_ARGS__), 0, __VA_ARGS__)


void mvl_add_directory_entry(LIBMVL_CONTEXT *ctx, LIBMVL_OFFSET64 offset, const char *tag);
void mvl_add_directory_entry_n(LIBMVL_CONTEXT *ctx, LIBMVL_OFFSET64 offset, const char *tag, LIBMVL_OFFSET64 tag_size);
LIBMVL_OFFSET64 mvl_write_directory(LIBMVL_CONTEXT *ctx);

LIBMVL_NAMED_LIST *mvl_create_named_list(int size);
void mvl_free_named_list(LIBMVL_NAMED_LIST *L);
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

#define mvl_vector_type(data)   (((LIBMVL_VECTOR_HEADER *)(data))->type)
#define mvl_vector_length(data)   (((LIBMVL_VECTOR_HEADER *)(data))->length)
#define mvl_vector_data(data)   ((((LIBMVL_VECTOR *)(data))->u))
#define mvl_vector_metadata_offset(data)   ((((LIBMVL_VECTOR_HEADER *)(data))->metadata))

/* These two convenience functions are meant for retrieving a few values, such as stored configuration parameters.
 * Only floating point and offset values are supported as output because they have intrinsic notion of invalid value.
 */

static inline double mvl_as_double(const LIBMVL_VECTOR *vec, long idx) 
{
if((idx<0) || (idx>=mvl_vector_length(vec)))return(NAN);

switch(mvl_vector_type(vec)) {
	case LIBMVL_VECTOR_DOUBLE:
		return(mvl_vector_data(vec).d[idx]);
	case LIBMVL_VECTOR_FLOAT:
		return(mvl_vector_data(vec).f[idx]);
	case LIBMVL_VECTOR_INT64:
		return(mvl_vector_data(vec).i64[idx]);
	case LIBMVL_VECTOR_INT32:
		return(mvl_vector_data(vec).i[idx]);
	default:
		return(NAN);
	}
}

static inline double mvl_as_double_default(const LIBMVL_VECTOR *vec, long idx, double def) 
{
if((idx<0) || (idx>=mvl_vector_length(vec)))return(def);

switch(mvl_vector_type(vec)) {
	case LIBMVL_VECTOR_DOUBLE:
		return(mvl_vector_data(vec).d[idx]);
	case LIBMVL_VECTOR_FLOAT:
		return(mvl_vector_data(vec).f[idx]);
	case LIBMVL_VECTOR_INT64:
		return(mvl_vector_data(vec).i64[idx]);
	case LIBMVL_VECTOR_INT32:
		return(mvl_vector_data(vec).i[idx]);
	default:
		return(def);
	}
}

static inline LIBMVL_OFFSET64 mvl_as_offset(const LIBMVL_VECTOR *vec, long idx) 
{
if((idx<0) || (idx>=mvl_vector_length(vec)))return(0);

switch(mvl_vector_type(vec)) {
	case LIBMVL_VECTOR_OFFSET64:
		return(mvl_vector_data(vec).offset[idx]);
	default:
		return(0);
	}
}

static inline double mvl_named_list_get_double(LIBMVL_NAMED_LIST *L, const void *data, long tag_length, const char *tag, long idx)
{
LIBMVL_VECTOR *vec;
LIBMVL_OFFSET64 ofs;
ofs=mvl_find_list_entry(L, tag_length, tag);
if(ofs==0)return(NAN);

vec=(LIBMVL_VECTOR *)&(((char *)data)[ofs]);
return(mvl_as_double(vec, idx));
}

static inline double mvl_named_list_get_double_default(LIBMVL_NAMED_LIST *L, const void *data, long tag_length, const char *tag, long idx, double def)
{
LIBMVL_VECTOR *vec;
LIBMVL_OFFSET64 ofs;
ofs=mvl_find_list_entry(L, tag_length, tag);
if(ofs==0)return(def);

vec=(LIBMVL_VECTOR *)&(((char *)data)[ofs]);
return(mvl_as_double_default(vec, idx, def));
}

static inline LIBMVL_OFFSET64 mvl_named_list_get_offset(LIBMVL_NAMED_LIST *L, const void *data, long tag_length, const char *tag, long idx)
{
LIBMVL_VECTOR *vec;
LIBMVL_OFFSET64 ofs;
ofs=mvl_find_list_entry(L, tag_length, tag);
if(ofs==0)return(0);

vec=(LIBMVL_VECTOR *)&(((char *)data)[ofs]);
return(mvl_as_offset(vec, idx));
}

static inline LIBMVL_OFFSET64 mvl_packed_list_get_entry_bytelength(LIBMVL_VECTOR *vec, LIBMVL_OFFSET64 idx)
{
LIBMVL_OFFSET64 start, stop, len;
if(mvl_vector_type(vec)!=LIBMVL_PACKED_LIST64)return -1;
len=mvl_vector_length(vec);
if((idx+1>=len) || (idx<0))return -1;
start=mvl_vector_data(vec).offset[idx];
stop=mvl_vector_data(vec).offset[idx+1];
return(stop-start);
}

/* This returns char even though the underlying type can be different - we just want the pointer */
static inline const char * mvl_packed_list_get_entry(LIBMVL_VECTOR *vec, const void *data, LIBMVL_OFFSET64 idx)
{
LIBMVL_OFFSET64 start, len;
if(mvl_vector_type(vec)!=LIBMVL_PACKED_LIST64)return NULL;
len=mvl_vector_length(vec);
if((idx+1>=len) || (idx<0))return NULL;
start=mvl_vector_data(vec).offset[idx];
return(&(((const char *)(data))[start]));
}

LIBMVL_OFFSET64 mvl_find_directory_entry(LIBMVL_CONTEXT *ctx, const char *tag);

/* This initializes context to use in-memory image of given length starting at data
 * the image could have been loaded via fread, or memory mapped
 */
void mvl_load_image(LIBMVL_CONTEXT *ctx, LIBMVL_OFFSET64 length, const void *data);

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
int mvl_sort_indices(LIBMVL_OFFSET64 indices_count, LIBMVL_OFFSET64 *indices, LIBMVL_OFFSET64 vec_count, LIBMVL_VECTOR **vec, void **vec_data, int sort_function);

#endif
