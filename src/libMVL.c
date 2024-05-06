
/* (c) Vladimir Dergachev 2019-2021 */

/*!  @file
 *   @brief core libMVL functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#ifndef __WIN32__
#include <alloca.h>
#else
#include <malloc.h>
#endif

#ifdef RMVL_PACKAGE
#include <R.h>
#include <Rinternals.h>
#endif

#include "libMVL.h"

static void *do_malloc(LIBMVL_OFFSET64 a, LIBMVL_OFFSET64 b)
{
void *r;
int i=0;
LIBMVL_OFFSET64 total_size;

/* basic sanity checks */
if(a<1)a=1;
if(b<1)b=1;
total_size=a*b;
if(total_size<1)total_size=1;

if(total_size/b<a) {
	#ifdef USING_R
		Rprintf("libMVL: *** INTERNAL ERROR: Could not allocate %llu chunks of %llu bytes each because of overflow %llu total)\n",a,b,a*b);
	#else
		fprintf(stderr,"libMVL: *** INTERNAL ERROR: Could not allocate %llu chunks of %llu bytes each because of overflow %llu total\n",a,b,a*b);
	#endif
	return(NULL);
	}

r=malloc(total_size);
while(r==NULL){
#ifdef USING_R
	Rprintf("libMVL: Could not allocate %llu chunks of %llu bytes each (%llu bytes total)\n",a,b,a*b);
#else
	fprintf(stderr,"libMVL: Could not allocate %llu chunks of %llu bytes each (%llu bytes total)\n",a,b,a*b);
#endif
//	if(i>args_info.memory_allocation_retries_arg)exit(-1);
	sleep(10);
	r=malloc(total_size);
	i++;
	}
//if(a*b>10e6)madvise(r, a*b, MADV_HUGEPAGE);
return r;
}

static inline char *memndup(const char *s, LIBMVL_OFFSET64 len)
{
char *p;
int i;
p=do_malloc(len+1, 1);
for(i=0;i<len;i++)p[i]=s[i];
p[len]=0;
return(p);
}

#ifdef __APPLE__

#else

#define HAVE_FTELLO

#endif

off_t do_ftello(FILE *f)
{
#ifdef HAVE_FTELLO
return(ftello(f));
#else
/* ftello() is broken on MacOS >= 10.15 */
fflush(f);
return(lseek(fileno(f), 0, SEEK_CUR));
#endif
}

#ifndef HAVE_POSIX_FALLOCATE

#if _POSIX_C_SOURCE >= 200112L
#define HAVE_POSIX_FALLOCATE 1
#else
#define HAVE_POSIX_FALLOCATE 0
#endif

#endif

static int do_fallocate(FILE *f, LIBMVL_OFFSET64 offset, LIBMVL_OFFSET64 len)
{
#if HAVE_POSIX_FALLOCATE

return(posix_fallocate(fileno(f), offset, len));

#else

off_t cur, end, i;
int err;
#ifndef FALLOCATE_BUF_SIZE
#define FALLOCATE_BUF_SIZE 512
#endif
char buf[FALLOCATE_BUF_SIZE];

cur=do_ftello(f);
if(cur<0)return(cur);

if((err=fseeko(f, 0, SEEK_END))<0) {
	return(err);
	}
	
end=do_ftello(f);
if(end<0)return(end);
	
if(end>=(offset+len))return(0);

memset(buf, 0, FALLOCATE_BUF_SIZE);

for(i=end;i<offset+len;i+=FALLOCATE_BUF_SIZE) {
	size_t cnt=offset+len-i;
	if(cnt>FALLOCATE_BUF_SIZE)cnt=FALLOCATE_BUF_SIZE;
	fwrite(buf, 1, cnt, f);
	}
	
if((err=fseeko(f, cur, SEEK_SET))<0) {
	return(err);
	}
return(0);
#endif
}


/*!  @brief Create MVL context 
 * 
 *   @return A pointer to allocated LIBMVL_CONTEXT structure
 */
LIBMVL_CONTEXT *mvl_create_context(void)
{
LIBMVL_CONTEXT *ctx;
//ctx=calloc(1, sizeof(*ctx));
ctx=do_malloc(1, sizeof(*ctx));
if(ctx==NULL)return(ctx);

ctx->error=0;
ctx->abort_on_error=1;
ctx->alignment=32;

//ctx->directory=do_malloc(ctx->dir_size, sizeof(*ctx->directory));
ctx->directory=mvl_create_named_list(100);
mvl_recompute_named_list_hash(ctx->directory);
ctx->directory_offset=-1;

ctx->character_class_offset=0;

ctx->cached_strings=mvl_create_named_list(32);

ctx->flags=0;

#ifdef HAVE_POSIX_FALLOCATE
ctx->flags|=LIBMVL_CTX_FLAG_HAVE_POSIX_FALLOCATE;
#endif

#ifdef HAVE_FTELLO
ctx->flags|=LIBMVL_CTX_FLAG_HAVE_FTELLO;
#endif

return(ctx);
}

/*! @brief Release memory associated with MVL context
 *  @param ctx pointer to context previously allocated with mvl_create_context()
 */
void mvl_free_context(LIBMVL_CONTEXT *ctx)
{
mvl_free_named_list(ctx->directory);
// for(LIBMVL_OFFSET64 i=0;i<ctx->dir_free;i++)
// 	free(ctx->directory[i].tag);
// free(ctx->directory);
mvl_free_named_list(ctx->cached_strings);
free(ctx);
}

void mvl_set_error(LIBMVL_CONTEXT *ctx, int error)
{
ctx->error=error;
if(ctx->abort_on_error) {
#ifdef USING_R
	Rprintf("*** ERROR: libMVL code %d: %s\n", error, mvl_strerror(ctx));
#else
	fprintf(stderr, "*** ERROR: libMVL code %d: %s\n", error, mvl_strerror(ctx));
	exit(-1);
#endif
	}
}

/*! @brief Obtain description of error code
 *  @param ctx pointer to context previously allocated with mvl_create_context()
 *  @return pointer to C string which memory is owned by the context
 */
const char * mvl_strerror(LIBMVL_CONTEXT *ctx)
{
switch(ctx->error) {
	case 0:  
		return("no error");
	case LIBMVL_ERR_FAIL_PREAMBLE:
		return("invalid preamble");
	case LIBMVL_ERR_FAIL_POSTAMBLE:
		return("invalid postamble");
	case LIBMVL_ERR_UNKNOWN_TYPE	:
		return("unknown type");
	case LIBMVL_ERR_FAIL_VECTOR:
		return("unknown type");
	case LIBMVL_ERR_INCOMPLETE_WRITE:
		return("incomplete write");
	case LIBMVL_ERR_INVALID_SIGNATURE:
		return("invalid signature");
	case LIBMVL_ERR_WRONG_ENDIANNESS:
		return("wrong endianness");
	case LIBMVL_ERR_EMPTY_DIRECTORY:
		return("empty MVL directory");
	case LIBMVL_ERR_INVALID_DIRECTORY:
		return("invalid MVL directory");
	case LIBMVL_ERR_FTELL:
		return("call to ftell() failed");
	case LIBMVL_ERR_CORRUPT_POSTAMBLE:
		return("corrupt postamble");
	case LIBMVL_ERR_INVALID_ATTR_LIST:
		return("invalid attribute list");
	case LIBMVL_ERR_INVALID_OFFSET:
		return("invalid offset");
	case LIBMVL_ERR_INVALID_ATTR:
		return("invalid attributes");
	case LIBMVL_ERR_CANNOT_SEEK:
		return("seek() call failed");
	case LIBMVL_ERR_INVALID_PARAMETER:
		return("invalid parameter");
	case LIBMVL_ERR_INVALID_LENGTH:
		return("invalid length");
	case LIBMVL_ERR_INVALID_EXTENT_INDEX:
		return("invalid extent index");
	case LIBMVL_ERR_CORRUPT_PACKED_LIST:
		return("corrupt packed list");
	default:
		return("unknown error");
	
	}
}

void mvl_write(LIBMVL_CONTEXT *ctx, LIBMVL_OFFSET64 length, const void *data)
{
LIBMVL_OFFSET64 n;
n=fwrite(data, 1, length, ctx->f);
if(n<length)mvl_set_error(ctx, LIBMVL_ERR_INCOMPLETE_WRITE);
}

void mvl_rewrite(LIBMVL_CONTEXT *ctx, LIBMVL_OFFSET64 offset, LIBMVL_OFFSET64 length, const void *data)
{
LIBMVL_OFFSET64 n;
off_t cur;
cur=do_ftello(ctx->f);
if(cur<0) {
	mvl_set_error(ctx, LIBMVL_ERR_FTELL);
	return;
	}
if(fseeko(ctx->f, offset, SEEK_SET)<0) {
	mvl_set_error(ctx, LIBMVL_ERR_CANNOT_SEEK);
	return;
	}
n=fwrite(data, 1, length, ctx->f);
if(n<length)mvl_set_error(ctx, LIBMVL_ERR_INCOMPLETE_WRITE);
if(fseeko(ctx->f, cur, SEEK_SET)<0) {
	mvl_set_error(ctx, LIBMVL_ERR_CANNOT_SEEK);
	return;
	}
}

void mvl_write_preamble(LIBMVL_CONTEXT *ctx)
{
memset(&(ctx->tmp_preamble), 0, sizeof(ctx->tmp_preamble));
memcpy(ctx->tmp_preamble.signature, LIBMVL_SIGNATURE, 4);
ctx->tmp_preamble.endianness=LIBMVL_ENDIANNESS_FLAG;
ctx->tmp_preamble.alignment=ctx->alignment;
mvl_write(ctx, sizeof(ctx->tmp_preamble), &ctx->tmp_preamble);
}

void mvl_write_postamble(LIBMVL_CONTEXT *ctx)
{
memset(&(ctx->tmp_postamble), 0, sizeof(ctx->tmp_postamble));
ctx->tmp_postamble.directory=ctx->directory_offset;
#ifdef MVL_OLD_DIRECTORY
ctx->tmp_postamble.type=LIBMVL_VECTOR_POSTAMBLE1;
#else
ctx->tmp_postamble.type=LIBMVL_VECTOR_POSTAMBLE2;
#endif
mvl_write(ctx, sizeof(ctx->tmp_postamble), &ctx->tmp_postamble);
}

/*!  @brief Write complete MVL vector 
 *   @param ctx MVL context pointer that has been initialized for writing
 *   @param type MVL data type
 *   @param length number of elements to write
 *   @param data  pointer to data
 *   @param metadata an optional offset to previously written metadata. Specify LIBMVL_NO_METADATA if not needed
 *   @return an offset into the file, suitable for adding to MVL file directory, or to other MVL objects
 */
LIBMVL_OFFSET64 mvl_write_vector(LIBMVL_CONTEXT *ctx, int type, LIBMVL_OFFSET64 length, const void *data, LIBMVL_OFFSET64 metadata)
{
LIBMVL_OFFSET64 byte_length;
int padding;
unsigned char *zeros;
off_t offset;

memset(&(ctx->tmp_vh), 0, sizeof(ctx->tmp_vh));

byte_length=length*mvl_element_size(type);
if(byte_length<=0) {
	mvl_set_error(ctx, LIBMVL_ERR_UNKNOWN_TYPE);
	return(LIBMVL_NULL_OFFSET);
	}
padding=ctx->alignment-((byte_length+sizeof(ctx->tmp_vh)) & (ctx->alignment-1));
padding=padding & (ctx->alignment-1);

ctx->tmp_vh.length=length;
ctx->tmp_vh.type=type;
ctx->tmp_vh.metadata=metadata;

offset=do_ftello(ctx->f);

if(offset<0) {
	perror("mvl_write_vector");
	mvl_set_error(ctx, LIBMVL_ERR_FTELL);
	return(LIBMVL_NULL_OFFSET);
	}

mvl_write(ctx, sizeof(ctx->tmp_vh), &ctx->tmp_vh);
mvl_write(ctx, byte_length, data);

if(padding>0) {
	zeros=alloca(padding);
	memset(zeros, 0, padding);
	mvl_write(ctx, padding, zeros);
	}

return(offset);
}

/*!  @brief Begin write of MVL vector. This is only needed if the vector has to be written in parts, such as due to memory constraints.
 *   @param ctx MVL context pointer that has been initialized for writing
 *   @param type MVL data type
 *   @param expected_length number of elements in the fully written vector
 *   @param length number of elements to write
 *   @param data  pointer to data
 *   @param metadata an optional offset to previously written metadata. Specify LIBMVL_NO_METADATA if not needed
 *   @return an offset into the file, suitable for adding to MVL file directory, or to other MVL objects
 */
LIBMVL_OFFSET64 mvl_start_write_vector(LIBMVL_CONTEXT *ctx, int type, LIBMVL_OFFSET64 expected_length, LIBMVL_OFFSET64 length, const void *data, LIBMVL_OFFSET64 metadata)
{
LIBMVL_OFFSET64 byte_length, total_byte_length;
int padding;
unsigned char *zeros;
off_t offset;

if(length>expected_length) {
	mvl_set_error(ctx, LIBMVL_ERR_INVALID_PARAMETER);
	return(LIBMVL_NULL_OFFSET);
	}


memset(&(ctx->tmp_vh), 0, sizeof(ctx->tmp_vh));

switch(type) {
	case LIBMVL_VECTOR_CSTRING:
	case LIBMVL_VECTOR_UINT8:
		byte_length=length;
		total_byte_length=expected_length;
		break;
	case LIBMVL_VECTOR_INT32:
	case LIBMVL_VECTOR_FLOAT:
		byte_length=length*4;
		total_byte_length=expected_length*4;
		break;
	case LIBMVL_VECTOR_INT64:
	case LIBMVL_VECTOR_DOUBLE:
	case LIBMVL_VECTOR_OFFSET64:
	case LIBMVL_PACKED_LIST64:
		byte_length=length*8;
		total_byte_length=expected_length*8;
		break;
	default:
		mvl_set_error(ctx, LIBMVL_ERR_UNKNOWN_TYPE);
		return(LIBMVL_NULL_OFFSET);
	}
padding=ctx->alignment-((total_byte_length+sizeof(ctx->tmp_vh)) & (ctx->alignment-1));
padding=padding & (ctx->alignment-1);

ctx->tmp_vh.length=expected_length;
ctx->tmp_vh.type=type;
ctx->tmp_vh.metadata=metadata;

offset=do_ftello(ctx->f);

if(offset<0) {
	perror("mvl_write_vector");
	mvl_set_error(ctx, LIBMVL_ERR_FTELL);
	return(LIBMVL_NULL_OFFSET);
	}
	
if(do_fallocate(ctx->f, offset, sizeof(ctx->tmp_vh)+total_byte_length+padding)) {
	mvl_set_error(ctx, LIBMVL_ERR_INCOMPLETE_WRITE);
	return(LIBMVL_NULL_OFFSET);
	}

mvl_write(ctx, sizeof(ctx->tmp_vh), &ctx->tmp_vh);
if(byte_length>0)mvl_write(ctx, byte_length, data);
if(total_byte_length>byte_length) {
	if(fseeko(ctx->f, total_byte_length-byte_length, SEEK_CUR)<0) {
		mvl_set_error(ctx, LIBMVL_ERR_CANNOT_SEEK);
		return(LIBMVL_NULL_OFFSET);
		}
	}

if(padding>0) {
	zeros=alloca(padding);
	memset(zeros, 0, padding);
	mvl_write(ctx, padding, zeros);
	}
	
return(offset);
}

/*!  @brief Write more data to MVL vector that has been previously created with mvl_start_write_vector()
 *   @param ctx MVL context pointer that has been initialized for writing
 *   @param type MVL data type
 *   @param base_offset the offset returned by mvl_start_write_vector()
 *   @param idx  index of of first element pointed to by data
 *   @param length number of elements to write
 *   @param data  pointer to data
 */
void mvl_rewrite_vector(LIBMVL_CONTEXT *ctx, int type, LIBMVL_OFFSET64 base_offset, LIBMVL_OFFSET64 idx, long length, const void *data)
{
LIBMVL_OFFSET64 byte_length, elt_size;

elt_size=mvl_element_size(type);
byte_length=length*elt_size;

if(byte_length>0)mvl_rewrite(ctx, base_offset+elt_size*idx+sizeof(ctx->tmp_vh), byte_length, data);
}

/*!  @brief Write MVL vector that contains data at specific indices. The indices can repeat, and can themselves be stored in memory mapped MVL file.
 *   @param ctx MVL context pointer that has been initialized for writing
 *   @param index_count number of indices to process, this will determine the length of the new vector
 *   @param indices array of indices into vector vec
 *   @param vec a pointer to fully formed MVL vector, such as from mapped MVL file
 *   @param data  pointer to data of previously mapped MVL library
 *   @param metadata an optional offset to previously written metadata. Specify LIBMVL_NO_METADATA if not needed
 *   @param max_buffer maximum size of buffer to hold in-flight data. Recommend to set to at least 10MB for efficiency.
 *   @return an offset into the file, suitable for adding to MVL file directory, or to other MVL objects
 */
LIBMVL_OFFSET64 mvl_indexed_copy_vector(LIBMVL_CONTEXT *ctx, LIBMVL_OFFSET64 index_count, const LIBMVL_OFFSET64 *indices, const LIBMVL_VECTOR *vec, const void *data, LIBMVL_OFFSET64 metadata, LIBMVL_OFFSET64 max_buffer)
{
LIBMVL_OFFSET64 char_length, vec_length, i, m, k, i_start, char_start, char_buf_length, vec_buf_length, N;
LIBMVL_OFFSET64 offset, char_offset;
unsigned char *char_buffer;
void *vec_buffer;

switch(mvl_vector_type(vec)) {
	case LIBMVL_PACKED_LIST64:
		vec_length=index_count+1;
		char_length=0;
		for(i=0;i<index_count;i++) {
			char_length+=mvl_packed_list_get_entry_bytelength(vec, indices[i]);
			}
		break;
	default:
		vec_length=index_count;
		char_length=0;
	}
	
vec_buf_length=vec_length;
if(vec_buf_length*mvl_element_size(mvl_vector_type(vec))>max_buffer) {
	vec_buf_length=max_buffer/mvl_element_size(mvl_vector_type(vec));
	}
if(vec_buf_length<50)vec_buf_length=50;
vec_buffer=do_malloc(vec_buf_length, mvl_element_size(mvl_vector_type(vec)));

offset=mvl_start_write_vector(ctx, mvl_vector_type(vec), vec_length, 0, NULL, metadata);

if(mvl_vector_type(vec)==LIBMVL_PACKED_LIST64) {
	char_buf_length=char_length;
	if(char_buf_length>max_buffer)char_buf_length=max_buffer;
	if(char_buf_length<100)char_buf_length=100;
	char_buffer=do_malloc(char_buf_length, 1);
	char_offset=mvl_start_write_vector(ctx, LIBMVL_VECTOR_UINT8, char_length, 0, NULL, LIBMVL_NO_METADATA);
	i=char_offset+sizeof(LIBMVL_VECTOR_HEADER);
	mvl_rewrite_vector(ctx, mvl_vector_type(vec), offset, 0, 1, &i);
	} else {
	char_buf_length=0;
	char_buffer=NULL;
	}

i_start=0;
char_start=0;
while(i_start<index_count) {
	switch(mvl_vector_type(vec)) {
		case LIBMVL_PACKED_LIST64: {
			LIBMVL_OFFSET64 *po=(LIBMVL_OFFSET64 *)vec_buffer;
			i=mvl_packed_list_get_entry_bytelength(vec, indices[i_start]);
			//fprintf(stderr, "packed_list i_start=%lld char_start=%lld\n", i_start, char_start);
			if(i>=char_buf_length) {
				mvl_rewrite_vector(ctx, LIBMVL_VECTOR_UINT8, char_offset, char_start, i, mvl_packed_list_get_entry(vec, data, indices[i_start]));
				po[0]=char_start+char_offset+sizeof(LIBMVL_VECTOR_HEADER);				
				mvl_rewrite_vector(ctx, mvl_vector_type(vec), offset, i_start+1, 1, po);
				i_start++;
				break;
				}
			N=0;
			for(i=0;(i<char_buf_length) && (N<vec_buf_length) && (i_start+N<index_count);i+=mvl_packed_list_get_entry_bytelength(vec, indices[i_start+N]), N++) {
				}
			if(i>char_buf_length)N--;
// 			fprintf(stderr, "buffer plan N=%lld vec_buf_length=%lld i=%lld char_buf_length=%lld\n", N, vec_buf_length, i, char_buf_length);
			//fprintf(stderr, "packed_list i=%lld N=%lld char_buf_len=%lld vec_buf_len=%lld index_count=%lld\n", i, N, char_buf_length, vec_buf_length, index_count);
// 			if(N>vec_buf_length) {
// 				fprintf(stderr, "*** INTERNAL ERROR: buffer overrun N=%lld vec_buf_length=%lld\n", N, vec_buf_length);
// 				}
			k=0;
			for(i=0;i<N;i++) {
				m=mvl_packed_list_get_entry_bytelength(vec, indices[i_start+i]);
				memcpy(&(char_buffer[k]), mvl_packed_list_get_entry(vec, data, indices[i_start+i]), m);
				k+=m;
				po[i]=char_offset+char_start+sizeof(LIBMVL_VECTOR_HEADER)+k;
				}
// 			if(k>char_buf_length) {
// 				fprintf(stderr, "*** INTERNAL ERROR: buffer overrun k=%lld char_buf_length=%lld N=%lld vec_buf_length=%lld\n", k, char_buf_length, N, vec_buf_length);
// 				}
			//fprintf(stderr, "packed_list i=%lld N=%lld k=%lld char_buf_len=%lld vec_buf_len=%lld\n", i, N, k, char_buf_length, vec_buf_length);
			mvl_rewrite_vector(ctx, LIBMVL_VECTOR_UINT8, char_offset, char_start, k, char_buffer);
			mvl_rewrite_vector(ctx, mvl_vector_type(vec), offset, i_start+1, N, po);
			i_start+=N;
			char_start+=k;
			break;
			}
		case LIBMVL_VECTOR_UINT8: {
			unsigned char *pb=(unsigned char *)vec_buffer;
			N=index_count-i_start;
			if(N>vec_buf_length)N=vec_buf_length;
			for(i=0;i<N;i++) {
				pb[i]=mvl_vector_data_uint8(vec)[indices[i+i_start]];
				}
			mvl_rewrite_vector(ctx, mvl_vector_type(vec), offset, i_start, N, pb);
			i_start+=N;
			break;
			}
		case LIBMVL_VECTOR_INT32: {
			int *pi=(int *)vec_buffer;
			N=index_count-i_start;
			if(N>vec_buf_length)N=vec_buf_length;
			for(i=0;i<N;i++) {
				pi[i]=mvl_vector_data_int32(vec)[indices[i+i_start]];
				}
			mvl_rewrite_vector(ctx, mvl_vector_type(vec), offset, i_start, N, pi);
			i_start+=N;
			break;
			}
		case LIBMVL_VECTOR_INT64: {
			long long *pi=(long long *)vec_buffer;
			N=index_count-i_start;
			if(N>vec_buf_length)N=vec_buf_length;
			for(i=0;i<N;i++) {
				pi[i]=mvl_vector_data_int64(vec)[indices[i+i_start]];
				}
			mvl_rewrite_vector(ctx, mvl_vector_type(vec), offset, i_start, N, pi);
			i_start+=N;
			break;
			}
		case LIBMVL_VECTOR_FLOAT: {
			float *pf=(float *)vec_buffer;
			N=index_count-i_start;
			if(N>vec_buf_length)N=vec_buf_length;
			for(i=0;i<N;i++) {
				pf[i]=mvl_vector_data_float(vec)[indices[i+i_start]];
				}
			mvl_rewrite_vector(ctx, mvl_vector_type(vec), offset, i_start, N, pf);
			i_start+=N;
			break;
			}
		case LIBMVL_VECTOR_DOUBLE: {
			double *pd=(double *)vec_buffer;
			N=index_count-i_start;
			if(N>vec_buf_length)N=vec_buf_length;
			for(i=0;i<N;i++) {
				pd[i]=mvl_vector_data_double(vec)[indices[i+i_start]];
				}
			mvl_rewrite_vector(ctx, mvl_vector_type(vec), offset, i_start, N, pd);
			i_start+=N;
			break;
			}
			
		default:
			mvl_set_error(ctx, LIBMVL_ERR_UNKNOWN_TYPE);
			i_start=index_count;
		}
	}

free(vec_buffer);
free(char_buffer);
return(offset);
}

/*!  @brief Write complete MVL vector concatenating data from many vectors or arrays
 *   @param ctx MVL context pointer that has been initialized for writing
 *   @param type MVL data type
 *   @param nvec number of arrays to concatenate
 *   @param lengths array of lengths of individual vectors
 *   @param data  array of pointers to vector data
 *   @param metadata an optional offset to previously written metadata. Specify LIBMVL_NO_METADATA if not needed
 *   @return an offset into the file, suitable for adding to MVL file directory, or to other MVL objects
 */
LIBMVL_OFFSET64 mvl_write_concat_vectors(LIBMVL_CONTEXT *ctx, int type, long nvec, const long *lengths, void **data, LIBMVL_OFFSET64 metadata)
{
LIBMVL_OFFSET64 byte_length, length;
int padding, item_size;
unsigned char *zeros;
off_t offset;
int i;

length=0;
for(i=0;i<nvec;i++)length+=lengths[i];

memset(&(ctx->tmp_vh), 0, sizeof(ctx->tmp_vh));

item_size=mvl_element_size(type);
if(item_size<=0) {
	mvl_set_error(ctx, LIBMVL_ERR_UNKNOWN_TYPE);
	return(LIBMVL_NULL_OFFSET);
	}
byte_length=length*item_size;
padding=ctx->alignment-((byte_length+sizeof(ctx->tmp_vh)) & (ctx->alignment-1));
padding=padding & (ctx->alignment-1);

ctx->tmp_vh.length=length;
ctx->tmp_vh.type=type;
ctx->tmp_vh.metadata=metadata;

offset=do_ftello(ctx->f);

if((long long)offset<0) {
	perror("mvl_write_vector");
	mvl_set_error(ctx, LIBMVL_ERR_FTELL);
	}

mvl_write(ctx, sizeof(ctx->tmp_vh), &ctx->tmp_vh);
for(i=0;i<nvec;i++)
	mvl_write(ctx, lengths[i]*item_size, data[i]);

if(padding>0) {
	zeros=alloca(padding);
	memset(zeros, 0, padding);
	mvl_write(ctx, padding, zeros);
	}

return(offset);
}

/*! @brief Write a single C string. In particular, this is handy for providing metadata tags 
 *   @param ctx MVL context pointer that has been initialized for writing
 *   @param length string length. Set to -1 to be computed automatically.
 *   @param data  string data
 *   @param metadata an optional offset to previously written metadata. Specify LIBMVL_NO_METADATA if not needed
 *   @return an offset into the file, suitable for adding to MVL file directory, or to other MVL objects
 */
LIBMVL_OFFSET64 mvl_write_string(LIBMVL_CONTEXT *ctx, long length, const char *data, LIBMVL_OFFSET64 metadata)
{
if(length<0)length=strlen(data);
return(mvl_write_vector(ctx, LIBMVL_VECTOR_CSTRING, length, data, metadata));
}

/*! @brief Write a single C string if it has not been written before, otherwise return offset to previously written object. In particular, this is handy for providing metadata tags 
 *   @param ctx MVL context pointer that has been initialized for writing
 *   @param length string length. Set to -1 to be computed automatically.
 *   @param data  string data
 *   @return an offset into the file, suitable for adding to MVL file directory, or to other MVL objects
 */
LIBMVL_OFFSET64 mvl_write_cached_string(LIBMVL_CONTEXT *ctx, long length, const char *data)
{
LIBMVL_OFFSET64 ofs;
if(length<0)length=strlen(data);
ofs=mvl_find_list_entry(ctx->cached_strings, length, data);
if(ofs!=LIBMVL_NULL_OFFSET)return(ofs);

ofs=mvl_write_vector(ctx, LIBMVL_VECTOR_CSTRING, length, data, LIBMVL_NO_METADATA);
mvl_add_list_entry(ctx->cached_strings, length, data, ofs);
return(ofs);
}

LIBMVL_OFFSET64 mvl_write_vector_inline(LIBMVL_CONTEXT *ctx, int type, int count, LIBMVL_OFFSET64 metadata, ...)
{
int i;
va_list ap;

va_start(ap, metadata);

switch(type) {
	case LIBMVL_VECTOR_CSTRING:
	case LIBMVL_VECTOR_UINT8: {
		char *data;
		data=alloca(count);
		for(i=0;i<count;i++)data[i]=va_arg(ap, int);
		va_end(ap);
		return(mvl_write_vector(ctx, type, count, data, metadata));
		break;
		}
	case LIBMVL_VECTOR_INT32: {
		int *data;
		data=alloca(count*sizeof(*data));
		for(i=0;i<count;i++)data[i]=va_arg(ap, int);
		va_end(ap);
		return(mvl_write_vector(ctx, type, count, data, metadata));
		break;
		}
	case LIBMVL_VECTOR_FLOAT: {
		float *data;
		data=alloca(count*sizeof(*data));
		for(i=0;i<count;i++)data[i]=va_arg(ap, double);
		va_end(ap);
		return(mvl_write_vector(ctx, type, count, data, metadata));
		break;
		}
	case LIBMVL_VECTOR_INT64: {
		long long *data;
		data=alloca(count*sizeof(*data));
		for(i=0;i<count;i++)data[i]=va_arg(ap, long long);
		va_end(ap);
		return(mvl_write_vector(ctx, type, count, data, metadata));
		break;
		}
	case LIBMVL_VECTOR_DOUBLE: {
		double *data;
		data=alloca(count*sizeof(*data));
		for(i=0;i<count;i++)data[i]=va_arg(ap, double);
		va_end(ap);
		return(mvl_write_vector(ctx, type, count, data, metadata));
		break;
		}
	case LIBMVL_VECTOR_OFFSET64: {
		LIBMVL_OFFSET64 *data;
		data=alloca(count*sizeof(*data));
		for(i=0;i<count;i++)data[i]=va_arg(ap, LIBMVL_OFFSET64);
		va_end(ap);
		return(mvl_write_vector(ctx, type, count, data, metadata));
		break;
		}
	default:
		mvl_set_error(ctx, LIBMVL_ERR_UNKNOWN_TYPE);
		return(LIBMVL_NULL_OFFSET);
	}
	
}

/*! @brief Write an array of strings as a packed list data type. This is convenient for storing a lot of different strings
 *   @param ctx MVL context pointer that has been initialized for writing
 *   @param count Number of strings to store
 *   @param str_size array of lengths of individual strings. If this is NULL string lengths are computed automatically. In addition, if any string length is -1 it is also computed automatically.
 *   @param str  point to array of strings
 *   @param metadata an optional offset to previously written metadata. Specify LIBMVL_NO_METADATA if not needed
 *   @return an offset into the file, suitable for adding to MVL file directory, or to other MVL objects
 */
LIBMVL_OFFSET64 mvl_write_packed_list(LIBMVL_CONTEXT *ctx, long count, const long *str_size,  unsigned char **str, LIBMVL_OFFSET64 metadata)
{
LIBMVL_OFFSET64 *ofsv, ofs1, ofs2, len1;
long *str_size2;
long i;
ofsv=do_malloc(count+1, sizeof(*ofsv));
str_size2=do_malloc(count, sizeof(*str_size2));

len1=0;
for(i=0;i<count;i++) {
	if((str_size==NULL) || (str_size[i]<0)) {
		str_size2[i]=strlen((char *)str[i]);
		} else {
		str_size2[i]=str_size[i];
		}
	len1+=str_size2[i];
	}
ofs1=mvl_write_concat_vectors(ctx, LIBMVL_VECTOR_UINT8, count, str_size2, (void **)str, LIBMVL_NO_METADATA);

ofsv[0]=ofs1+sizeof(LIBMVL_VECTOR_HEADER);
for(i=0;i<count;i++) {
	ofsv[i+1]=ofsv[i]+str_size2[i];
	}
	
ofs2=mvl_write_vector(ctx, LIBMVL_PACKED_LIST64, count+1, ofsv, metadata);
free(ofsv);
free(str_size2);
return(ofs2);
}

/*! @brief Get offset to metadata describing R-style character class - an array of strings. This is convenient for writing columns of strings to be analyzed with R - just provide this offset as the metadata field of mvl_write_packed_list()
 *   @param ctx MVL context pointer that has been initialized for writing
 *   @return an offset into the file, suitable for specifying as MVL object metadata
 */
LIBMVL_OFFSET64 mvl_get_character_class_offset(LIBMVL_CONTEXT *ctx)
{
LIBMVL_NAMED_LIST *L;
if(ctx->character_class_offset==0) {
	L=mvl_create_R_attributes_list(ctx, "character");
	ctx->character_class_offset=mvl_write_attributes_list(ctx, L);
	mvl_free_named_list(L);
	}
return(ctx->character_class_offset);
}

/*! @brief Add an entry to the top level directory of MVL file. 
 *   @param ctx MVL context pointer that has been initialized for writing
 *   @param offset directory entry value - typically an offset pointing to previously written MVL object
 *   @param tag  C string describing directory entry. When necessary, these can repeat, in which case the last written entry is retrieved first.
 */
void mvl_add_directory_entry(LIBMVL_CONTEXT *ctx, LIBMVL_OFFSET64 offset, const char *tag)
{
// LIBMVL_DIRECTORY_ENTRY *p;
// if(ctx->dir_free>=ctx->dir_size) {
// 	ctx->dir_size+=ctx->dir_size+10;
// 	
// 	p=do_malloc(ctx->dir_size, sizeof(*p));
// 	if(ctx->dir_free>0)memcpy(p, ctx->directory, ctx->dir_free*sizeof(*p));
// 	free(ctx->directory);
// 	ctx->directory=p;
// 	}
// //fprintf(stderr, "Adding entry %d \"%s\"=0x%016x\n", ctx->dir_free, tag, offset);
// ctx->directory[ctx->dir_free].offset=offset;
// ctx->directory[ctx->dir_free].tag=strdup(tag);
// ctx->dir_free++;
mvl_add_list_entry(ctx->directory, -1, tag, offset);
}

/*! @brief Add entry to the top level directory of MVL file. 
 *   @param ctx MVL context pointer that has been initialized for writing
 *   @param offset directory entry value - typically an offset pointing to previously written MVL object
 *   @param tag  string describing directory entry. When necessary, these can repeat, in which case the last written entry is retrieved first.
 *   @param tag_size  length of tag
 */
void mvl_add_directory_entry_n(LIBMVL_CONTEXT *ctx, LIBMVL_OFFSET64 offset, const char *tag, LIBMVL_OFFSET64 tag_size)
{
// LIBMVL_DIRECTORY_ENTRY *p;
// if(ctx->dir_free>=ctx->dir_size) {
// 	ctx->dir_size+=ctx->dir_size+10;
// 	
// 	p=do_malloc(ctx->dir_size, sizeof(*p));
// 	if(ctx->dir_free>0)memcpy(p, ctx->directory, ctx->dir_free*sizeof(*p));
// 	free(ctx->directory);
// 	ctx->directory=p;
// 	}
// ctx->directory[ctx->dir_free].offset=offset;
// ctx->directory[ctx->dir_free].tag=memndup(tag, tag_size);
// ctx->dir_free++;
mvl_add_list_entry(ctx->directory, tag_size, tag, offset);
}

/*! @brief Write out MVL file directory with entries collected so far. If this is called multiple times only the latest written directory is retrieved when MVL file is opened. It is an error to write out an empty directory.
 *   @param ctx MVL context pointer that has been initialized for writing
 *   @return an offset into the file where the directory was written
 */
LIBMVL_OFFSET64 mvl_write_directory(LIBMVL_CONTEXT *ctx)
{
LIBMVL_OFFSET64 *p;
LIBMVL_OFFSET64 offset;
off_t cur;

if(ctx->directory->free<1) {
	mvl_set_error(ctx, LIBMVL_ERR_EMPTY_DIRECTORY);
	return(0);
	}

#ifdef MVL_OLD_DIRECTORY
	
p=do_malloc(ctx->directory->free*2, sizeof(*p));
for(int i=0;i<ctx->directory->free;i++) {
// 	p[i]=mvl_write_vector(ctx, LIBMVL_VECTOR_UINT8,  strlen(ctx->directory[i].tag), ctx->directory[i].tag, LIBMVL_NO_METADATA);
// 	p[i+ctx->dir_free]=ctx->directory[i].offset;
	p[i]=mvl_write_vector(ctx, LIBMVL_VECTOR_UINT8,  ctx->directory->tag_length[i], ctx->directory->tag[i], LIBMVL_NO_METADATA);
	p[i+ctx->directory->free]=ctx->directory->offset[i];
	}

	
cur=do_ftello(ctx->f);

if((long long)cur<0) {
	perror("mvl_write_directory");
	mvl_set_error(ctx, LIBMVL_ERR_FTELL);
	}
offset=cur;

mvl_write_vector(ctx, LIBMVL_VECTOR_OFFSET64, 2*ctx->directory->free, p, LIBMVL_NO_METADATA);

free(p);

#else 
offset=mvl_write_named_list(ctx, ctx->directory);
#endif

ctx->directory_offset=offset;

return(offset);
}

/*! @brief Allocate and initialize structure for LIBMVL_NAMED_LIST
 *   @param size this can be set to large values if the final size of named list is known
 *   @return point to structure for LIBMVL_NAMED_LIST
 */
LIBMVL_NAMED_LIST *mvl_create_named_list(int size)
{
LIBMVL_NAMED_LIST *L;
L=do_malloc(1, sizeof(*L));
L->size=size;
L->free=0;
if(L->size<10)L->size=10;

L->offset=do_malloc(L->size, sizeof(*L->offset));
L->tag_length=do_malloc(L->size, sizeof(*L->tag_length));
L->tag=do_malloc(L->size, sizeof(*L->tag));

L->hash_size=0;
L->next_item=NULL;
L->first_item=NULL;

return(L);
}

/*! @brief Free structure for LIBMVL_NAMED_LIST
 *   @param L pointer to previously allocated LIBMVL_NAMED_LIST
 */
void mvl_free_named_list(LIBMVL_NAMED_LIST *L)
{
long i;
for(i=0;i<L->free;i++)free(L->tag[i]);
free(L->next_item);
free(L->first_item);
free(L->offset);
free(L->tag);
free(L->tag_length);
free(L);
}

/*! @brief Recompute named list hash
 *   @param L pointer to previously allocated LIBMVL_NAMED_LIST
 */
void mvl_recompute_named_list_hash(LIBMVL_NAMED_LIST *L)
{
LIBMVL_OFFSET64 mask;
if(L->hash_size<L->size) {
	LIBMVL_OFFSET64 hs=1;
	
	while(hs<L->size && hs)hs=hs<<1;
	
	L->hash_size=hs;
	free(L->next_item);
	free(L->first_item);
	
	/* This can only happen if L->size is greater than 2^63 - unlikely */
	if(hs==0) {
		L->next_item=NULL;
		L->first_item=NULL;
		return;
		}
	L->next_item=do_malloc(L->hash_size, sizeof(*L->next_item));
	L->first_item=do_malloc(L->hash_size, sizeof(*L->first_item));
	}
mask=L->hash_size-1;
for(LIBMVL_OFFSET64 i=0;i<L->hash_size;i++)L->first_item[i]=-1;
for(LIBMVL_OFFSET64 i=0;i<L->free;i++) {
	LIBMVL_OFFSET64 h=mvl_accumulate_hash64(MVL_SEED_HASH_VALUE, L->tag[i], L->tag_length[i]) & mask;
	L->next_item[i]=L->first_item[h];
	L->first_item[h]=i;	
	}
}

/*! @brief Add entry to LIBMVL_NAMED_LIST. The entry is always appended to the end.
 *   @param L pointer to previously allocated LIBMVL_NAMED_LIST
 *   @param tag_length size of tag
 *   @param tag string identifying entry - these can repeat.
 *   @param offset  64-bit value
 *   @return index of entry inside named list
 */
long mvl_add_list_entry(LIBMVL_NAMED_LIST *L, long tag_length, const char *tag, LIBMVL_OFFSET64 offset)
{
void *p;
long k;
if(L->free>=L->size) {
	L->size=2*L->size+10;
	
	p=do_malloc(L->size, sizeof(*L->offset));
	if(L->free>0)memcpy(p, L->offset, L->free*sizeof(*L->offset));
	free(L->offset);
	L->offset=p;
	
	p=do_malloc(L->size, sizeof(*L->tag_length));
	if(L->free>0)memcpy(p, L->tag_length, L->free*sizeof(*L->tag_length));
	free(L->tag_length);
	L->tag_length=p;
	
	p=do_malloc(L->size, sizeof(*L->tag));
	if(L->free>0)memcpy(p, L->tag, L->free*sizeof(*L->tag));
	free(L->tag);
	L->tag=p;
	}

if(L->hash_size && (L->free>=L->hash_size))mvl_recompute_named_list_hash(L);

k=L->free;
L->free++;
L->offset[k]=offset;
if(tag_length<0)tag_length=strlen(tag);
L->tag_length[k]=tag_length;
L->tag[k]=(unsigned char*)memndup(tag, tag_length);

if(L->hash_size>0) {
	LIBMVL_OFFSET64 mask=L->hash_size-1;
	LIBMVL_OFFSET64 h=mvl_accumulate_hash64(MVL_SEED_HASH_VALUE, (const unsigned char*)tag, tag_length) & mask;
	L->next_item[k]=L->first_item[h];
	L->first_item[h]=k;	
	}
return(k);
}

/*! @brief Find existing entry inside LIBMVL_NAMED_LIST. If several identically named entries exist this function returns last written value. Hash table is used if present.
 *   @param L pointer to previously allocated LIBMVL_NAMED_LIST
 *   @param tag_length size of tag
 *   @param tag string identifying entry - these can repeat.
 *   @return entry value
 */
LIBMVL_OFFSET64 mvl_find_list_entry(LIBMVL_NAMED_LIST *L, long tag_length, const char *tag)
{
long i, tl;
tl=tag_length;
if(tl<0)tl=strlen(tag);

if(L->hash_size>0) {
	/* Hash table present */
	LIBMVL_OFFSET64 mask=L->hash_size-1;
	LIBMVL_OFFSET64 h=mvl_accumulate_hash64(MVL_SEED_HASH_VALUE, (const unsigned char*)tag, tl) & mask;
	for(i=L->first_item[h]; i>=0; i=L->next_item[i]) {
		if(L->tag_length[i]!=tl)continue;
		if(!memcmp(L->tag[i], tag, tl)) {
			return(L->offset[i]);
			}
		}
	return(LIBMVL_NULL_OFFSET);
	}

for(i=0;i<L->free;i++) {
	if(L->tag_length[i]!=tl)continue;
	if(!memcmp(L->tag[i], tag, tl)) {
		return(L->offset[i]);
		}
	}
return(LIBMVL_NULL_OFFSET);
}


/*! @brief Create R-style attribute list for class given by R_class, which could be, for example, "data.frame"
 *   @param ctx MVL context pointer that has been initialized for writing
 *   @param R_class string identifying R class, such as "data.frame"
 *   @return pointer to LIBMVL_NAMED_LIST with allocated parameters
 */
LIBMVL_NAMED_LIST *mvl_create_R_attributes_list(LIBMVL_CONTEXT *ctx, const char *R_class)
{
LIBMVL_NAMED_LIST *L;
L=mvl_create_named_list(-1);
mvl_add_list_entry(L, -1, "MVL_LAYOUT", mvl_write_cached_string(ctx, -1, "R"));
mvl_add_list_entry(L, -1, "class", mvl_write_cached_string(ctx, -1, R_class));
return(L);
}

/*! @brief Write out R-style attribute list.
 *   @param ctx MVL context pointer that has been initialized for writing
 *   @param L previously created attributes list
 *   @return an offset into the file, suitable for use as vector metadata
 */
LIBMVL_OFFSET64 mvl_write_attributes_list(LIBMVL_CONTEXT *ctx, LIBMVL_NAMED_LIST *L)
{
LIBMVL_OFFSET64 *offsets, attr_offset;
long i;
offsets=do_malloc(2*L->free, sizeof(*offsets));

for(i=0;i<L->free;i++) {
	offsets[i]=mvl_write_cached_string(ctx, L->tag_length[i], (const char *)L->tag[i]);
	}
memcpy(&(offsets[L->free]), L->offset, L->free*sizeof(*offsets));

attr_offset=mvl_write_vector(ctx, LIBMVL_VECTOR_OFFSET64, 2*L->free, offsets, LIBMVL_NO_METADATA);

free(offsets);

return(attr_offset);
}

/*! @brief Write out named list. In R, this would be read back as list.
 *   @param ctx MVL context pointer that has been initialized for writing
 *   @param L previously created named list
 *   @return an offset into the file, suitable for adding to MVL file directory, or to other MVL objects
 */
LIBMVL_OFFSET64 mvl_write_named_list(LIBMVL_CONTEXT *ctx, LIBMVL_NAMED_LIST *L)
{
LIBMVL_OFFSET64 list_offset;
LIBMVL_NAMED_LIST *metadata;
	
metadata=mvl_create_R_attributes_list(ctx, "list");
//mvl_add_list_entry(metadata, -1, "names", mvl_write_vector(ctx, LIBMVL_VECTOR_OFFSET64, L->free, offsets, LIBMVL_NO_METADATA));
mvl_add_list_entry(metadata, -1, "names", mvl_write_packed_list(ctx, L->free, L->tag_length, L->tag, LIBMVL_NO_METADATA));

list_offset=mvl_write_vector(ctx, LIBMVL_VECTOR_OFFSET64, L->free, L->offset, mvl_write_attributes_list(ctx, metadata));

mvl_free_named_list(metadata);

return(list_offset);
}

/*! @brief Write out named list. In R, this would be read back as list with class attribute set to "cl"
 *   @param ctx MVL context pointer that has been initialized for writing
 *   @param L previously created named list
 *   @param cl character string describing list class
 *   @return an offset into the file, suitable for adding to MVL file directory, or to other MVL objects
 */
LIBMVL_OFFSET64 mvl_write_named_list2(LIBMVL_CONTEXT *ctx, LIBMVL_NAMED_LIST *L, char *cl)
{
LIBMVL_OFFSET64 list_offset;
LIBMVL_NAMED_LIST *metadata;
	
metadata=mvl_create_R_attributes_list(ctx, cl);
//mvl_add_list_entry(metadata, -1, "names", mvl_write_vector(ctx, LIBMVL_VECTOR_OFFSET64, L->free, offsets, LIBMVL_NO_METADATA));
mvl_add_list_entry(metadata, -1, "names", mvl_write_packed_list(ctx, L->free, L->tag_length, L->tag, LIBMVL_NO_METADATA));

list_offset=mvl_write_vector(ctx, LIBMVL_VECTOR_OFFSET64, L->free, L->offset, mvl_write_attributes_list(ctx, metadata));

mvl_free_named_list(metadata);

return(list_offset);
}

/*! @brief Write out named list in the style of R data frames. It is assumed that all entries of L are vectors with the same number of elements.
 *   @param ctx MVL context pointer that has been initialized for writing
 *   @param L previously created named list
 *   @param nrows number of elements in each entry of L. Note that packed lists should have length of nrows+1
 *   @param rownames names of individual rows. Set to 0 to omit.
 *   @return an offset into the file, suitable for adding to MVL file directory, or to other MVL objects
 */
LIBMVL_OFFSET64 mvl_write_named_list_as_data_frame(LIBMVL_CONTEXT *ctx, LIBMVL_NAMED_LIST *L, int nrows, LIBMVL_OFFSET64 rownames)
{
LIBMVL_OFFSET64 list_offset;
LIBMVL_NAMED_LIST *metadata;
	
metadata=mvl_create_R_attributes_list(ctx, "data.frame");
// mvl_add_list_entry(metadata, -1, "names", mvl_write_vector(ctx, LIBMVL_VECTOR_OFFSET64, L->free, offsets, LIBMVL_NO_METADATA));
mvl_add_list_entry(metadata, -1, "names", mvl_write_packed_list(ctx, L->free, L->tag_length, L->tag, LIBMVL_NO_METADATA));
mvl_add_list_entry(metadata, -1, "dim", MVL_WVEC(ctx, LIBMVL_VECTOR_INT32, nrows, (int)L->free));
if(rownames!=0)mvl_add_list_entry(metadata, -1, "rownames", rownames);


list_offset=mvl_write_vector(ctx, LIBMVL_VECTOR_OFFSET64, L->free, L->offset, mvl_write_attributes_list(ctx, metadata));

mvl_free_named_list(metadata);

return(list_offset);
}

/* This is meant to operate on memory mapped files */
/*! @brief Read back MVL attributes list, typically used to described metadata. This function also initialize hash table for fast access. This function does not check that the offsets stored in returned LIBMVL_NAMED_LIST data structure are valid, this should be done by the code that uses those offsets.
 *   @param ctx MVL context pointer
 *   @param data memory mapped data
 *   @param data_size size of memory mapped data
 *   @param metadata_offset metadata offset pointing to the previously written attributes
 *   @return NULL if there is no metadata, otherwise LIBMVL_NAMED_LIST populated with attributes
 */
LIBMVL_NAMED_LIST *mvl_read_attributes_list(LIBMVL_CONTEXT *ctx, const void *data, LIBMVL_OFFSET64 data_size, LIBMVL_OFFSET64 metadata_offset)
{
LIBMVL_NAMED_LIST *L;
long i, nattr;
char *p, *d;
int err;

if(metadata_offset==LIBMVL_NO_METADATA)return(NULL);

if((err=mvl_validate_vector(metadata_offset, data, data_size))!=0) {
	mvl_set_error(ctx, LIBMVL_ERR_INVALID_OFFSET);
	return(NULL);
	}

d=(char *)data;

if(mvl_vector_type(&(d[metadata_offset]))!=LIBMVL_VECTOR_OFFSET64) {
	mvl_set_error(ctx, LIBMVL_ERR_INVALID_OFFSET);
	return(NULL);
	}

p=&(d[metadata_offset]);

nattr=mvl_vector_length(p);
if(nattr==0)return(NULL);
if((nattr<0) || (nattr & 1)) {
	mvl_set_error(ctx, LIBMVL_ERR_INVALID_ATTR_LIST);
	return(NULL);
	}
nattr=nattr>>1;

L=mvl_create_named_list(nattr);
for(i=0;i<nattr;i++) {
	if((err=mvl_validate_vector(mvl_vector_data_offset(p)[i], data, data_size))!=0) {
		mvl_set_error(ctx, LIBMVL_ERR_INVALID_OFFSET);
		mvl_add_list_entry(L, 
			9, 
			"*CORRUPT*", 
			mvl_vector_data_offset(p)[i+nattr]);
		} else {
		mvl_add_list_entry(L, 
			mvl_vector_length(&(d[mvl_vector_data_offset(p)[i]])), 
			(const char *)mvl_vector_data_uint8(&(d[mvl_vector_data_offset(p)[i]])), 
			mvl_vector_data_offset(p)[i+nattr]);
		}
	}

mvl_recompute_named_list_hash(L);
return(L);
}

/* This is meant to operate on memory mapped files */
/*! @brief Read back MVL named list. This function also initialize hash table for fast access.
 *   @param ctx MVL context pointer
 *   @param data memory mapped data
 *   @param data_size size of memory mapped data
 *   @param offset offset into data where LIBMVL_NAMED_LIST begins
 *   @return NULL on error, otherwise LIBMVL_NAMED_LIST
 */
LIBMVL_NAMED_LIST *mvl_read_named_list(LIBMVL_CONTEXT *ctx, const void *data, LIBMVL_OFFSET64 data_size, LIBMVL_OFFSET64 offset)
{
LIBMVL_NAMED_LIST *L, *Lattr;
char *d;
LIBMVL_OFFSET64 names_ofs, tag_ofs;
long i, nelem;
int err;

if(offset==LIBMVL_NULL_OFFSET)return(NULL);

if((err=mvl_validate_vector(offset, data, data_size))!=0) {
	mvl_set_error(ctx, LIBMVL_ERR_INVALID_OFFSET);
	return(NULL);
	}

d=(char *)data;

if(mvl_vector_type(&(d[offset]))!=LIBMVL_VECTOR_OFFSET64){
	mvl_set_error(ctx, LIBMVL_ERR_INVALID_OFFSET);
	return(NULL);
	}

Lattr=mvl_read_attributes_list(ctx, data, data_size, mvl_vector_metadata_offset(&(d[offset])));
if(Lattr==NULL)return(NULL);
names_ofs=mvl_find_list_entry(Lattr, -1, "names");

if((err=mvl_validate_vector(names_ofs, data, data_size))!=0) {
	mvl_set_error(ctx, LIBMVL_ERR_INVALID_OFFSET);
	return(NULL);
	}

nelem=mvl_vector_length(&(d[offset]));

L=mvl_create_named_list(nelem);

switch(mvl_vector_type(&(d[names_ofs]))) {
	case LIBMVL_VECTOR_OFFSET64:
		if(nelem!=mvl_vector_length(&(d[names_ofs]))) {
			mvl_free_named_list(L);
			mvl_free_named_list(Lattr);
			mvl_set_error(ctx, LIBMVL_ERR_INVALID_ATTR);
			return(NULL);
			}
		for(i=0;i<nelem;i++) {
			tag_ofs=mvl_vector_data_offset(&(d[names_ofs]))[i];
			
			if((err=mvl_validate_vector(tag_ofs, data, data_size))!=0) {
				mvl_set_error(ctx, LIBMVL_ERR_INVALID_OFFSET);
				mvl_add_list_entry(L, 9, "*CORRUPT*", mvl_vector_data_offset(&(d[offset]))[i]);
				continue;
				}
				
			mvl_add_list_entry(L, mvl_vector_length(&(d[tag_ofs])), (const char *)mvl_vector_data_uint8(&(d[tag_ofs])), mvl_vector_data_offset(&(d[offset]))[i]);
			}
		break;
	case LIBMVL_PACKED_LIST64:
		if(nelem+1!=mvl_vector_length(&(d[names_ofs]))) {
			mvl_free_named_list(L);
			mvl_free_named_list(Lattr);
			mvl_set_error(ctx, LIBMVL_ERR_INVALID_ATTR);
			return(NULL);
			}
		for(i=0;i<nelem;i++) {
			if((err=mvl_packed_list_validate_entry((LIBMVL_VECTOR *)&(d[names_ofs]), d, data_size, i))!=0) {
				mvl_set_error(ctx, LIBMVL_ERR_CORRUPT_PACKED_LIST);
				mvl_add_list_entry(L, 9, "*CORRUPT*", mvl_vector_data_offset(&(d[offset]))[i]);
				continue;
				}
			mvl_add_list_entry(L, mvl_packed_list_get_entry_bytelength((LIBMVL_VECTOR *)&(d[names_ofs]), i), (const char *)mvl_packed_list_get_entry((LIBMVL_VECTOR *)&(d[names_ofs]), d, i), mvl_vector_data_offset(&(d[offset]))[i]);
			}
		break;
	default:
		mvl_free_named_list(L);
		mvl_free_named_list(Lattr);
		mvl_set_error(ctx, LIBMVL_ERR_INVALID_ATTR);
		return(NULL);
	}

mvl_free_named_list(Lattr);

mvl_recompute_named_list_hash(L);
return(L);
}

/*! @brief Prepare context for writing to file f
 *   @param ctx MVL context pointer
 *   @param f pointer to previously opened stdio.h FILE structure
 */
void mvl_open(LIBMVL_CONTEXT *ctx, FILE *f)
{
ctx->f=f;
mvl_write_preamble(ctx);
}

/*! @brief Write out MVL file directory and postable and close file
 *   @param ctx MVL context pointer
 */
void mvl_close(LIBMVL_CONTEXT *ctx)
{
mvl_write_directory(ctx);
mvl_write_postamble(ctx);
fflush(ctx->f);
ctx->f=NULL;
}

LIBMVL_OFFSET64 mvl_directory_length(const void *data)
{
LIBMVL_VECTOR_HEADER *p=(LIBMVL_VECTOR_HEADER *)data;
if(p->type!=LIBMVL_VECTOR_OFFSET64) {
	return(0);
	}
if(p->length &1) {
	return 0;
	}
return(p->length>>1);
}

// LIBMVL_OFFSET64 mvl_directory_tag(const void *data, int i)
// {
// LIBMVL_VECTOR *p=(LIBMVL_VECTOR *)data;
// return(mvl_vector_data_offset(p)[i]);
// }
// 
// LIBMVL_OFFSET64 mvl_directory_entry(void *data, int i)
// {
// LIBMVL_VECTOR *p=(LIBMVL_VECTOR *)data;
// return(mvl_vector_data_offset(p)[i+(p->header.length>>1)]);
// }

/*! @brief Find entry in MVL file directory
 *   @param ctx MVL context pointer
 *   @param tag character string identifying entry
 *   @return offset into file the entry points to
 */
LIBMVL_OFFSET64 mvl_find_directory_entry(LIBMVL_CONTEXT *ctx, const char *tag)
{
// int i;
// for(i=ctx->dir_free-1;i>=0;i--) {
// 	if(!strcmp(tag, ctx->directory[i].tag))return(ctx->directory[i].offset);
// 	}
// return(0);
return(mvl_find_list_entry(ctx->directory, -1, tag));
}

/*! @brief Initilize MVL context to operate with memory mapped area data.
 *   @param ctx MVL context pointer
 *   @param length size of memory mapped data, in bytes
 *   @param data pointer to the beginning of memory mapped area
 */
void mvl_load_image(LIBMVL_CONTEXT *ctx, LIBMVL_OFFSET64 length, const void *data)
{
LIBMVL_PREAMBLE *pr=(LIBMVL_PREAMBLE *)data;
LIBMVL_POSTAMBLE *pa=(LIBMVL_POSTAMBLE *)&(((unsigned char *)data)[length-sizeof(LIBMVL_POSTAMBLE)]);
LIBMVL_VECTOR *dir, *a;
LIBMVL_OFFSET64 k;
int i;
int err;

if(strncmp(pr->signature, LIBMVL_SIGNATURE, 4)) {
	mvl_set_error(ctx, LIBMVL_ERR_INVALID_SIGNATURE);
	return;
	}

if(pr->endianness!=LIBMVL_ENDIANNESS_FLAG) {
	mvl_set_error(ctx, LIBMVL_ERR_WRONG_ENDIANNESS);
	return;
	}

//fprintf(stderr, "Reading MVL directory at offset 0x%08llx\n", pa->directory);

mvl_free_named_list(ctx->directory);

switch(pa->type) {
	case LIBMVL_VECTOR_POSTAMBLE2:
		if((err=mvl_validate_vector(pa->directory, data, length))<0) {
			ctx->directory=mvl_create_named_list(100);
			mvl_set_error(ctx, LIBMVL_ERR_CORRUPT_POSTAMBLE);
			return;
			}

		ctx->directory=mvl_read_named_list(ctx, data, length, pa->directory);
		break;
#ifdef MVL_OLD_DIRECTORY
	case LIBMVL_VECTOR_POSTAMBLE1:
		dir=(LIBMVL_VECTOR *)&(((unsigned char *)data)[pa->directory]);
		k=dir->header.length>>1;

		ctx->directory=mvl_create_named_list(k);

		// for(i=0;i<ctx->dir_free;i++) {
		// 	free(ctx->directory[i].tag);
		// 	ctx->directory[i].tag=NULL;
		// 	ctx->directory[i].offset=0;
		// 	}

		// ctx->dir_free=dir->header.length>>1;
		// //fprintf(stderr, "Reading MVL with %ld directory entries\n", ctx->dir_free);
		// if(ctx->dir_free >= ctx->dir_size) {
		// 	ctx->dir_size=ctx->dir_free;
		// 	free(ctx->directory);
		// 	ctx->directory=do_malloc(ctx->dir_size, sizeof(*ctx->directory));
		// 	}
			
		// for(i=0;i<ctx->dir_free;i++) {
		// 	ctx->directory[i].offset=mvl_vector_data(dir).offset[i+ctx->dir_free];
		// 	a=(LIBMVL_VECTOR *)&(((unsigned char *)data)[mvl_vector_data(dir).offset[i]]);
		// 	ctx->directory[i].tag=memndup(mvl_vector_data(a).b, a->header.length);
		// 	}
		for(i=0;i<k;i++) {
			a=(LIBMVL_VECTOR *)&(((unsigned char *)data)[mvl_vector_data_offset(dir)[i]]);
			
			mvl_add_list_entry(ctx->directory, a->header.length, (const char *)mvl_vector_data_uint8(a), mvl_vector_data_offset(dir)[i+k]); 
			}
		mvl_recompute_named_list_hash(ctx->directory);
		break;
#endif
	default:
		ctx->directory=mvl_create_named_list(100);
		mvl_set_error(ctx, LIBMVL_ERR_CORRUPT_POSTAMBLE);
		return;
	}
}

typedef struct {
	LIBMVL_VECTOR **vec;
	void **data; /* This is needed for packed vectors */
	LIBMVL_OFFSET64 nvec;
	} MVL_SORT_INFO;

typedef struct {
	LIBMVL_OFFSET64 index;
	MVL_SORT_INFO *info;
	} MVL_SORT_UNIT;
	
	
int mvl_equals(MVL_SORT_UNIT *a, MVL_SORT_UNIT *b)
{
LIBMVL_OFFSET64 i, ai, bi;
LIBMVL_OFFSET64 N=a->info->nvec;
LIBMVL_VECTOR *avec, *bvec;
ai=a->index;
bi=b->index;
for(i=0;i<N;i++) {
	avec=a->info->vec[i];
	bvec=b->info->vec[i];
		
	switch(mvl_vector_type(avec)) {
		case LIBMVL_VECTOR_CSTRING:
		case LIBMVL_VECTOR_UINT8: {
			unsigned char ad, bd;
			if(mvl_vector_type(bvec)!=mvl_vector_type(avec))return 0;
			ad=mvl_vector_data_uint8(avec)[ai];
			bd=mvl_vector_data_uint8(bvec)[bi];
			if(ad!=bd)return 0;
			break;
			}
		case LIBMVL_VECTOR_INT32: {
			int ad;
			ad=mvl_vector_data_int32(avec)[ai];
			switch(mvl_vector_type(bvec)) {
				case LIBMVL_VECTOR_INT32: {
					int bd;
					bd=mvl_vector_data_int32(bvec)[bi];
					if(ad!=bd)return 0;
					break;
					}
				case LIBMVL_VECTOR_INT64: {
					long long bd;
					bd=mvl_vector_data_int64(bvec)[bi];
					if(ad!=bd)return 0;
					break;
					}
				default:
					return 0;
					break;
				}
			break;
			}
		case LIBMVL_VECTOR_INT64: {
			long long ad;
			ad=mvl_vector_data_int64(avec)[ai];
			switch(mvl_vector_type(bvec)) {
				case LIBMVL_VECTOR_INT32: {
					int bd;
					bd=mvl_vector_data_int32(bvec)[bi];
					if(ad!=bd)return 0;
					break;
					}
				case LIBMVL_VECTOR_INT64: {
					long long bd;
					bd=mvl_vector_data_int64(bvec)[bi];
					if(ad!=bd)return 0;
					break;
					}
				default:
					return 0;
					break;
				}
			break;
			}
		case LIBMVL_VECTOR_FLOAT: {
			float ad;
			ad=mvl_vector_data_float(avec)[ai];
			switch(mvl_vector_type(bvec)) {
				case LIBMVL_VECTOR_FLOAT: {
					float bd;
					bd=mvl_vector_data_float(bvec)[bi];
					if(ad!=bd)return 0;
					break;
					}
				case LIBMVL_VECTOR_DOUBLE: {
					double bd;
					bd=mvl_vector_data_double(bvec)[bi];
					if((double)ad!=bd)return 0;
					break;
					}
				default:
					return 0;
					break;
				}
			break;
			}
		case LIBMVL_VECTOR_DOUBLE: {
			double ad;
			ad=mvl_vector_data_double(avec)[ai];
			switch(mvl_vector_type(bvec)) {
				case LIBMVL_VECTOR_FLOAT: {
					double bd;
					bd=mvl_vector_data_float(bvec)[bi];
					if(ad!=bd)return 0;
					break;
					}
				case LIBMVL_VECTOR_DOUBLE: {
					double bd;
					bd=mvl_vector_data_double(bvec)[bi];
					if(ad!=bd)return 0;
					break;
					}
				default:
					return 0;
					break;
				}
			break;
			}
		case LIBMVL_VECTOR_OFFSET64: {
			LIBMVL_OFFSET64 ad, bd;
			if(mvl_vector_type(bvec)!=mvl_vector_type(avec))return 0;
			ad=mvl_vector_data_offset(avec)[ai];
			bd=mvl_vector_data_offset(bvec)[bi];
			if(ad!=bd)return 0;
			break;
			}
		case LIBMVL_PACKED_LIST64: {
			LIBMVL_OFFSET64 al, bl, nn;
			const unsigned char *ad, *bd;
			if(mvl_vector_type(bvec)!=mvl_vector_type(avec))return 0;
			al=mvl_packed_list_get_entry_bytelength(avec, ai);
			bl=mvl_packed_list_get_entry_bytelength(bvec, bi);
			ad=mvl_packed_list_get_entry(avec, a->info->data[i], ai);
			bd=mvl_packed_list_get_entry(bvec, b->info->data[i], bi);
			if(al!=bl)return 0;
			nn=al;
			for(LIBMVL_OFFSET64 j=0;j<nn;j++) {
				if(ad[j]!=bd[j])return 0;
				}
			break;
			}
		default:
			return(0);
		}
	}
return 1;
}

/* The comparison functions below use absolute index as a last resort in comparison which preserves order for identical entries.
 * This makes these functions unsuitable to check equality
 * They also assume that a->info==b->info
 */
	
int mvl_lexicographic_cmp(MVL_SORT_UNIT *a, MVL_SORT_UNIT *b)
{
LIBMVL_OFFSET64 i, ai, bi;
LIBMVL_OFFSET64 N=a->info->nvec;
LIBMVL_VECTOR *vec;
ai=a->index;
bi=b->index;
for(i=0;i<N;i++) {
	vec=a->info->vec[i];
		
	switch(mvl_vector_type(vec)) {
		case LIBMVL_VECTOR_CSTRING:
		case LIBMVL_VECTOR_UINT8: {
			unsigned char ad, bd;
			ad=mvl_vector_data_uint8(vec)[ai];
			bd=mvl_vector_data_uint8(vec)[bi];
			if(ad<bd)return -1;
			if(ad>bd)return 1;
			break;
			}
		case LIBMVL_VECTOR_INT32: {
			int ad, bd;
			ad=mvl_vector_data_int32(vec)[ai];
			bd=mvl_vector_data_int32(vec)[bi];
			if(ad<bd)return -1;
			if(ad>bd)return 1;
			break;
			}
		case LIBMVL_VECTOR_FLOAT: {
			float ad, bd;
			ad=mvl_vector_data_float(vec)[ai];
			bd=mvl_vector_data_float(vec)[bi];
			if(ad<bd)return -1;
			if(ad>bd)return 1;
			break;
			}
		case LIBMVL_VECTOR_INT64: {
			long long ad, bd;
			ad=mvl_vector_data_int64(vec)[ai];
			bd=mvl_vector_data_int64(vec)[bi];
			if(ad<bd)return -1;
			if(ad>bd)return 1;
			break;
			}
		case LIBMVL_VECTOR_DOUBLE: {
			double ad, bd;
			ad=mvl_vector_data_double(vec)[ai];
			bd=mvl_vector_data_double(vec)[bi];
			if(ad<bd)return -1;
			if(ad>bd)return 1;
			break;
			}
		case LIBMVL_VECTOR_OFFSET64: {
			LIBMVL_OFFSET64 ad, bd;
			ad=mvl_vector_data_offset(vec)[ai];
			bd=mvl_vector_data_offset(vec)[bi];
			if(ad<bd)return -1;
			if(ad>bd)return 1;
			break;
			}
		case LIBMVL_PACKED_LIST64: {
			LIBMVL_OFFSET64 al, bl, nn;
			const unsigned char *ad, *bd;
			al=mvl_packed_list_get_entry_bytelength(vec, ai);
			bl=mvl_packed_list_get_entry_bytelength(vec, bi);
			ad=mvl_packed_list_get_entry(vec, a->info->data[i], ai);
			bd=mvl_packed_list_get_entry(vec, b->info->data[i], bi);
			nn=al;
			if(bl<nn)nn=bl;
			for(LIBMVL_OFFSET64 j=0;j<nn;j++) {
				if(ad[j]<bd[j])return -1;
				if(ad[j]>bd[j])return 1;
				}
			if(al<bl)return -1;
			if(al>bl)return 1;
			break;
			}
		default:
			if(ai<bi)return -1;
			if(ai>bi)return 1;
			return(0);
		}
	}
if(ai<bi)return -1;
if(ai>bi)return 1;
return 0;
}

int mvl_lexicographic_desc_cmp(MVL_SORT_UNIT *a, MVL_SORT_UNIT *b)
{
LIBMVL_OFFSET64 i, ai, bi;
LIBMVL_OFFSET64 N=a->info->nvec;
LIBMVL_VECTOR *vec;
ai=a->index;
bi=b->index;
for(i=0;i<N;i++) {
	vec=a->info->vec[i];
		
	switch(mvl_vector_type(vec)) {
		case LIBMVL_VECTOR_CSTRING:
		case LIBMVL_VECTOR_UINT8: {
			unsigned char ad, bd;
			ad=mvl_vector_data_uint8(vec)[ai];
			bd=mvl_vector_data_uint8(vec)[bi];
			if(ad<bd)return 1;
			if(ad>bd)return -1;
			break;
			}
		case LIBMVL_VECTOR_INT32: {
			int ad, bd;
			ad=mvl_vector_data_int32(vec)[ai];
			bd=mvl_vector_data_int32(vec)[bi];
			if(ad<bd)return 1;
			if(ad>bd)return -1;
			break;
			}
		case LIBMVL_VECTOR_FLOAT: {
			float ad, bd;
			ad=mvl_vector_data_float(vec)[ai];
			bd=mvl_vector_data_float(vec)[bi];
			if(ad<bd)return 1;
			if(ad>bd)return -1;
			break;
			}
		case LIBMVL_VECTOR_INT64: {
			long long ad, bd;
			ad=mvl_vector_data_int64(vec)[ai];
			bd=mvl_vector_data_int64(vec)[bi];
			if(ad<bd)return 1;
			if(ad>bd)return -1;
			break;
			}
		case LIBMVL_VECTOR_DOUBLE: {
			double ad, bd;
			ad=mvl_vector_data_double(vec)[ai];
			bd=mvl_vector_data_double(vec)[bi];
			if(ad<bd)return 1;
			if(ad>bd)return -1;
			break;
			}
		case LIBMVL_VECTOR_OFFSET64: {
			LIBMVL_OFFSET64 ad, bd;
			ad=mvl_vector_data_offset(vec)[ai];
			bd=mvl_vector_data_offset(vec)[bi];
			if(ad<bd)return 1;
			if(ad>bd)return -1;
			break;
			}
		case LIBMVL_PACKED_LIST64: {
			LIBMVL_OFFSET64 al, bl, nn;
			const unsigned char *ad, *bd;
			al=mvl_packed_list_get_entry_bytelength(vec, ai);
			bl=mvl_packed_list_get_entry_bytelength(vec, bi);
			ad=mvl_packed_list_get_entry(vec, a->info->data[i], ai);
			bd=mvl_packed_list_get_entry(vec, b->info->data[i], bi);
			nn=al;
			if(bl<nn)nn=bl;
			for(LIBMVL_OFFSET64 j=0;j<nn;j++) {
				if(ad[j]<bd[j])return 1;
				if(ad[j]>bd[j])return -1;
				}
			if(al<bl)return 1;
			if(al>bl)return -1;
			break;
			}
		default:
			if(ai<bi)return 1;
			if(ai>bi)return -1;
			return(0);
		}
	}
if(ai<bi)return 1;
if(ai>bi)return -1;
return 0;
}

/*
 * This function sorts indices into a list of vectors so that the resulting permutation is ordered.
 * The vector should all be the same length N, except LIBMVL_PACKED_LIST64 which should N+1 - this provides the same number of elements.
 * The indices are from 0 to N-1 and can repeat.
 * 
 * vec_data is the pointer to mapped data range where offsets point. This is needed only for vectors of type LIBMVL_PACKED_LIST64.
 * You can set vec_data to NULL if LIBMVL_PACKED_LIST64 vectors are not present. Also entries vec_data[i] can be NULL if the corresponding vector is not of type
 * LIBMVL_PACKED_LIST64
 * 
 * This function return 0 on successful sort. If no vectors are supplies (vec_count==0) the indices are unchanged and the sort is considered successful.
 */
int mvl_sort_indices1(LIBMVL_OFFSET64 indices_count, LIBMVL_OFFSET64 *indices, LIBMVL_OFFSET64 vec_count, LIBMVL_VECTOR **vec, void **vec_data, int sort_function)
{
MVL_SORT_UNIT *units;
MVL_SORT_INFO info;
LIBMVL_OFFSET64 i, N;

if(vec_count<1)return 0;

info.data=vec_data;
info.vec=vec;
info.nvec=vec_count;

units=do_malloc(indices_count, sizeof(*units));

N=mvl_vector_nentries(vec[0]);
//fprintf(stderr, "vec_count=%d N=%d\n", vec_count, N);
for(i=1;i<vec_count;i++) {
	if(mvl_vector_type(vec[i])==LIBMVL_PACKED_LIST64) {
		if(mvl_vector_length(vec[i])!=N+1)return -1;
		if(vec_data==NULL)return -1;
		if(vec_data[i]==NULL)return -1;
		continue;
		}
	if(mvl_vector_length(vec[i])!=N)return -1;
	}

for(i=0;i<indices_count;i++) {
	units[i].info=&info;
	if(indices[i]>=N)return -1;
	units[i].index=indices[i];
	}

switch(sort_function) {
	case LIBMVL_SORT_LEXICOGRAPHIC:
		qsort(units, indices_count, sizeof(*units), (int (*)(const void *, const void *))mvl_lexicographic_cmp);
		break;
	case LIBMVL_SORT_LEXICOGRAPHIC_DESC:
		qsort(units, indices_count, sizeof(*units), (int (*)(const void *, const void *))mvl_lexicographic_desc_cmp);
		break;
	default:
		break;
	}
for(i=0;i<indices_count;i++) {
	indices[i]=units[i].index;
	}
	
free(units);
return 0;
}

/*! @brief This function is used to compute 64 bit hash of vector values
 * array hash[] is passed in and contains the result of the computation
 * 
 * Integer indices are computed by value, so that 100 produces the same hash whether it is stored as INT32 or INT64.
 * 
 * Floats and doubles are trickier - we can guarantee that the hash of a float promoted to a double is the same as the hash of the original float, but not the reverse.
 * 
 * @param indices_count  total number of indices 
 * @param indices an array of indices into provided vectors
 * @param hash a previously allocated array of length indices_count that the computed hashes will be written into
 * @param vec_count the number of LIBMVL_VECTORS considered as columns in a table
 * @param vec an array of pointers to LIBMVL_VECTORS considered as columns in a table
 * @param vec_data an array of pointers to memory mapped areas those LIBMVL_VECTORs derive from. This allows computing hash from vectors drawn from different MVL files
 * @param flags flags specifying whether to initialize or finalize hash
 */
int mvl_hash_indices(LIBMVL_OFFSET64 indices_count, const LIBMVL_OFFSET64 *indices, LIBMVL_OFFSET64 *hash, LIBMVL_OFFSET64 vec_count, LIBMVL_VECTOR **vec, void **vec_data, int flags)
{
LIBMVL_OFFSET64 i, j, N;

if(flags & LIBMVL_INIT_HASH) {
	for(i=0;i<indices_count;i++) {
		hash[i]=MVL_SEED_HASH_VALUE;
		}
	}

if(vec_count<1)return 0;

N=mvl_vector_length(vec[0]);
//fprintf(stderr, "vec_count=%d N=%d\n", vec_count, N);
if(mvl_vector_type(vec[0])==LIBMVL_PACKED_LIST64)N--;
for(i=1;i<vec_count;i++) {
	if(mvl_vector_type(vec[i])==LIBMVL_PACKED_LIST64) {
		if(mvl_vector_length(vec[i])!=N+1)return -1;
		if(vec_data==NULL)return -2;
		if(vec_data[i]==NULL)return -3;
		continue;
		}
	if(mvl_vector_length(vec[i])!=N)return -4;
	}
	
for(i=0;i<indices_count;i++) {
	if(indices[i]>=N)return -5;
	}

for(j=0;j<vec_count;j++) {
	switch(mvl_vector_type(vec[j])) {
		case LIBMVL_VECTOR_CSTRING:
		case LIBMVL_VECTOR_UINT8: 
			for(i=0;i<indices_count;i++) {
				hash[i]=mvl_accumulate_hash64(hash[i], (const unsigned char *)&(mvl_vector_data_uint8(vec[j])[indices[i]]), 1);
				}
			break;
		case LIBMVL_VECTOR_INT32:
			for(i=0;i<indices_count;i++) {
				hash[i]=mvl_accumulate_int32_hash64(hash[i], &(mvl_vector_data_int32(vec[j])[indices[i]]), 1);
				}
			break;
		case LIBMVL_VECTOR_INT64:
			for(i=0;i<indices_count;i++) {
				hash[i]=mvl_accumulate_int64_hash64(hash[i], &(mvl_vector_data_int64(vec[j])[indices[i]]), 1);
				}
			break;
		case LIBMVL_VECTOR_FLOAT:
			for(i=0;i<indices_count;i++) {
				hash[i]=mvl_accumulate_float_hash64(hash[i], &(mvl_vector_data_float(vec[j])[indices[i]]), 1);
				}
			break;
		case LIBMVL_VECTOR_DOUBLE:
			for(i=0;i<indices_count;i++) {
				hash[i]=mvl_accumulate_double_hash64(hash[i], &(mvl_vector_data_double(vec[j])[indices[i]]), 1);
				}
			break;
		case LIBMVL_VECTOR_OFFSET64: /* TODO: we might want to do something more clever here */
			for(i=0;i<indices_count;i++) {
				hash[i]=mvl_accumulate_hash64(hash[i], (const unsigned char *)&(mvl_vector_data_int64(vec[j])[indices[i]]), 8);
				}
			break;
		case LIBMVL_PACKED_LIST64: {
			if(vec_data==NULL)return -6;
			if(vec_data[j]==NULL)return -7;
			for(i=0;i<indices_count;i++) {
				hash[i]=mvl_accumulate_hash64(hash[i], mvl_packed_list_get_entry(vec[j], vec_data[j], indices[i]), mvl_packed_list_get_entry_bytelength(vec[j], indices[i]));
				}
			break;
			}
		default:
			return(-1);
		}
	}
	
if(flags & LIBMVL_FINALIZE_HASH) {
	for(i=0;i<indices_count;i++) {
		hash[i]=mvl_randomize_bits64(hash[i]);
		}
	}
	
return 0;
}

/*! @brief This function is used to compute 64 bit hash of vector values
 * array hash[] is passed in and contains the result of the computation
 * 
 * Integer indices are computed by value, so that 100 produces the same hash whether it is stored as INT32 or INT64.
 * 
 * Floats and doubles are trickier - we can guarantee that the hash of a float promoted to a double is the same as the hash of the original float, but not the reverse.
 * 
 * @param i0  starting index to hash 
 * @param i1  first index to not hash
 * @param hash a previously allocated array of length (i1-i0) that the computed hashes will be written into
 * @param vec_count the number of LIBMVL_VECTORS considered as columns in a table
 * @param vec an array of pointers to LIBMVL_VECTORS considered as columns in a table
 * @param vec_data an array of pointers to memory mapped areas those LIBMVL_VECTORs derive from. This allows computing hash from vectors drawn from different MVL files
 * @param flags flags specifying whether to initialize or finalize hash
 */
int mvl_hash_range(LIBMVL_OFFSET64 i0, LIBMVL_OFFSET64 i1, LIBMVL_OFFSET64 *hash, LIBMVL_OFFSET64 vec_count, LIBMVL_VECTOR **vec, void **vec_data, int flags)
{
LIBMVL_OFFSET64 i, j, N, indices_count;

indices_count=i1-i0;

if(flags & LIBMVL_INIT_HASH) {
	for(i=0;i<indices_count;i++) {
		hash[i]=MVL_SEED_HASH_VALUE;
		}
	}

if(vec_count<1 || (i1<=i0))return 0;

N=mvl_vector_length(vec[0]);
//fprintf(stderr, "vec_count=%d N=%d\n", vec_count, N);
if(mvl_vector_type(vec[0])==LIBMVL_PACKED_LIST64)N--;
for(i=1;i<vec_count;i++) {
	if(mvl_vector_type(vec[i])==LIBMVL_PACKED_LIST64) {
		if(mvl_vector_length(vec[i])!=N+1)return -1;
		if(vec_data==NULL)return -2;
		if(vec_data[i]==NULL)return -3;
		continue;
		}
	if(mvl_vector_length(vec[i])!=N)return -4;
	}

if(i0>=N || i1>=N)return(-5);

for(j=0;j<vec_count;j++) {
	switch(mvl_vector_type(vec[j])) {
		case LIBMVL_VECTOR_CSTRING:
		case LIBMVL_VECTOR_UINT8: 
			for(i=0;i<indices_count;i++) {
				hash[i]=mvl_accumulate_hash64(hash[i], (const unsigned char *)&(mvl_vector_data_uint8(vec[j])[i+i0]), 1);
				}
			break;
		case LIBMVL_VECTOR_INT32:
			for(i=0;i<indices_count;i++) {
				hash[i]=mvl_accumulate_int32_hash64(hash[i], &(mvl_vector_data_int32(vec[j])[i+i0]), 1);
				}
			break;
		case LIBMVL_VECTOR_INT64:
			for(i=0;i<indices_count;i++) {
				hash[i]=mvl_accumulate_int64_hash64(hash[i], &(mvl_vector_data_int64(vec[j])[i+i0]), 1);
				}
			break;
		case LIBMVL_VECTOR_FLOAT:
			for(i=0;i<indices_count;i++) {
				hash[i]=mvl_accumulate_float_hash64(hash[i], &(mvl_vector_data_float(vec[j])[i+i0]), 1);
				}
			break;
		case LIBMVL_VECTOR_DOUBLE:
			for(i=0;i<indices_count;i++) {
				hash[i]=mvl_accumulate_double_hash64(hash[i], &(mvl_vector_data_double(vec[j])[i+i0]), 1);
				}
			break;
		case LIBMVL_VECTOR_OFFSET64: /* TODO: we might want to do something more clever here */
			for(i=0;i<indices_count;i++) {
				hash[i]=mvl_accumulate_hash64(hash[i], (const unsigned char *)&(mvl_vector_data_int64(vec[j])[i+i0]), 8);
				}
			break;
		case LIBMVL_PACKED_LIST64: {
			if(vec_data==NULL)return -6;
			if(vec_data[j]==NULL)return -7;
			for(i=0;i<indices_count;i++) {
				hash[i]=mvl_accumulate_hash64(hash[i], mvl_packed_list_get_entry(vec[j], vec_data[j], i+i0), mvl_packed_list_get_entry_bytelength(vec[j], i+i0));
				}
			break;
			}
		default:
			return(-1);
		}
	}
	
if(flags & LIBMVL_FINALIZE_HASH) {
	for(i=0;i<indices_count;i++) {
		hash[i]=mvl_randomize_bits64(hash[i]);
		}
	}
	
return 0;
}

/*! @brief
 *  Compute suggested size of hash map given the number of entries to hash. Hash map size should always be a power of 2.
 *  @param hash_count expected number of items to hash
 *  @return suggested hash map size
 */
LIBMVL_OFFSET64 mvl_compute_hash_map_size(LIBMVL_OFFSET64 hash_count)
{
LIBMVL_OFFSET64 hash_map_size;

if(hash_count & (1LLU<<63))return 0; /* Too big */

hash_map_size=1;
while(hash_map_size<hash_count) {
	hash_map_size=hash_map_size<<1;
	}
return(hash_map_size);
}

/*! @brief Create HASH_MAP structure. 
 * 
 *  This creates default HASH_MAP structure with all members allocated with new arrays. In some situations, such as to save memory it is possible to reuse existing arrays by specifying hm->flags appropriately. In such case, one should not use this constructor and instead create the structure manually.
 *  @param max_index_count expected number of entries to hash
 *  @return pointer to allocated HASH_MAP structure
 */
HASH_MAP *mvl_allocate_hash_map(LIBMVL_OFFSET64 max_index_count)
{
HASH_MAP *hm;

hm=do_malloc(1, sizeof(*hm));
hm->hash_count=0;
hm->hash_size=max_index_count;
hm->hash_map_size=mvl_compute_hash_map_size(max_index_count);

hm->hash=do_malloc(hm->hash_size, sizeof(hm->hash));
hm->hash_map=do_malloc(hm->hash_map_size, sizeof(*hm->hash_map));
hm->first=do_malloc(hm->hash_size, sizeof(*hm->first));
hm->next=do_malloc(hm->hash_size, sizeof(*hm->next));

hm->vec_count=0;

hm->flags=MVL_FLAG_OWN_HASH | MVL_FLAG_OWN_HASH_MAP | MVL_FLAG_OWN_FIRST | MVL_FLAG_OWN_NEXT;

return(hm);
}

/*! @brief Free allocated HASH_MAP
 *  @param hash_map a pointer to previously allocated hash_map structure
 */
void mvl_free_hash_map(HASH_MAP *hash_map)
{
if(hash_map->flags & MVL_FLAG_OWN_HASH)free(hash_map->hash);
if(hash_map->flags & MVL_FLAG_OWN_HASH_MAP)free(hash_map->hash_map);
if(hash_map->flags & MVL_FLAG_OWN_FIRST)free(hash_map->first);
if(hash_map->flags & MVL_FLAG_OWN_NEXT)free(hash_map->next);
if(hash_map->flags & MVL_FLAG_OWN_VEC_TYPES)free(hash_map->vec_types);

hash_map->hash_size=0;
hash_map->hash_map_size=0;
hash_map->vec_count=0;
free(hash_map);
}

/*! @brief Compute hash map. This assumes that hm->hash array has been populated with hm->hash_count hashes computed with mvl_hash_indices().
 *  @param hm a pointer to HASH_MAP structure
 */
void mvl_compute_hash_map(HASH_MAP *hm)
{
LIBMVL_OFFSET64 i, k, hash_mask, N_first, hash_count;
LIBMVL_OFFSET64 hash_map_size;
LIBMVL_OFFSET64 *hash_map, *next, *first, *hash;
 
hash_count=hm->hash_count;
hash=hm->hash;
hash_map=hm->hash_map;
first=hm->first;
next=hm->next;

hash_map_size=hm->hash_map_size;
hash_mask=hash_map_size-1;

for(i=0;i<hash_map_size;i++) {
	hash_map[i]=~0LLU;
	}

if(hash_mask & hash_map_size) {
	N_first=0;
	for(i=0;i<hash_count;i++) {
		k=hash[i] % hash_map_size;
		if(hash_map[k]==~0LLU) {
			hash_map[k]=i;
			first[N_first]=i;
			next[i]=~0LLU;
			N_first++;
			continue;
			}
		next[i]=hash_map[k];
		hash_map[k]=i;
		}
	for(i=0;i<N_first;i++) {
		k=hash[first[i]] % hash_map_size;
		first[i]=hash_map[k];
		}
	} else {
	N_first=0;
	for(i=0;i<hash_count;i++) {
		k=hash[i] & hash_mask;
		if(hash_map[k]==~0LLU) {
			hash_map[k]=i;
			first[N_first]=i;
			next[i]=~0LLU;
			N_first++;
			continue;
			}
		next[i]=hash_map[k];
		hash_map[k]=i;
		}
	for(i=0;i<N_first;i++) {
		k=hash[first[i]] & hash_mask;
		first[i]=hash_map[k];
		}
	}
hm->first_count=N_first;
}

/*! @brief Find count of matches between hashes of two sets. 
 * 
 * This function is useful to find the upper limit on the number of possible matches, so one can allocate arrays for the result or plan computation in some other way.
 *  @param key_count number of key hashes
 *  @param key_hash an array of key hashes to query
 *  @param hm a pointer to HASH_MAP structure
 *  @return number of matches
 */
LIBMVL_OFFSET64 mvl_hash_match_count(LIBMVL_OFFSET64 key_count, const LIBMVL_OFFSET64 *key_hash, HASH_MAP *hm)
{
LIBMVL_OFFSET64 i, k, hash_mask, match_count;
LIBMVL_OFFSET64 *hash_map, *next, *hash;
LIBMVL_OFFSET64 hash_map_size;


hash_map_size=hm->hash_map_size;
hash=hm->hash;
hash_map=hm->hash_map;
next=hm->next;

hash_mask=hash_map_size-1;

match_count=0;
if(hash_map_size & hash_mask) {
	for(i=0;i<key_count;i++) {
		k=hash_map[key_hash[i] % hash_map_size];
		while(k!=~0LLU) {
			if(hash[k]==key_hash[i])match_count++;
			k=next[k];
			}
		}
	} else {
	for(i=0;i<key_count;i++) {
		k=hash_map[key_hash[i] & hash_mask];
		while(k!=~0LLU) {
			if(hash[k]==key_hash[i])match_count++;
			k=next[k];
			}
		}
	}
return(match_count);
}

/* Find indices of keys in set of hashes, using hash map. 
 * Only the first matching hash is reported.
 * If not found the index is set to ~0 (0xfff...fff)
 * Output is in key_indices 
 */
void mvl_find_first_hashes(LIBMVL_OFFSET64 key_count, const LIBMVL_OFFSET64 *key_hash, LIBMVL_OFFSET64 *key_indices, HASH_MAP *hm)
{
LIBMVL_OFFSET64 i, k, hash_mask, hash_map_size;
LIBMVL_OFFSET64 *hash_map, *next, *hash;

hash=hm->hash;
hash_map_size=hm->hash_map_size;
hash_map=hm->hash_map;
next=hm->next;

hash_mask=hash_map_size-1;

if(hash_map_size & hash_mask) {
	for(i=0;i<key_count;i++) {
		k=hash_map[key_hash[i] % hash_map_size];
		while(k!=~0LLU) {
			if(hash[k]==key_hash[i])break;
			k=next[k];
			}
		key_indices[i]=k;
		}
	} else {
	for(i=0;i<key_count;i++) {
		k=hash_map[key_hash[i] & hash_mask];
		while(k!=~0LLU) {
			if(hash[k]==key_hash[i])break;
			k=next[k];
			}
		key_indices[i]=k;
		}
	}
}

/* This function computes pairs of merge indices. The pairs are stored in key_match_indices[] and match_indices[].
 * All arrays should be provided by the caller. The size of match_indices arrays is computed with mvl_hash_match_count()
 * An auxiliary array key_last of length key_indices_count stores the stop before index (in terms of matches array). 
 * In particular the total number of matches is given by key_last[key_indices_count-1]
 */
/*! @brief Compute pairs of merge indices. This is similar to JOIN operation in SQL.
 * 
 * This function takes two table like sets of vectors as input. The vectors in each table set have to be of equal number of elements. 
 * We also take two index arrays specifying rows in each table set. We then find pairs of indices where the rows are identical.
 * 
 * The output is returned in pair of preallocated arrays key_match_indices and match_indices. The pairs are arrange in stretches of identical "key" rows. 
 * Those stretches are described by key_last array.
 * 
 *  @param key_indices_count  number of entries in key_indices array
 *  @param key_indices an array with indices into "key" table-like vector set
 *  @param key_vec_count number of vectors in "key" table set
 *  @param key_vec an array of vectors in "key" table set
 *  @param key_vec_data an array of pointers to memory mapped areas those "key" vectors derive from. This allows computing hash from vectors drawn from different MVL files
 *  @param key_hash an array of hashes of "key" vectors computed with mvl_hash_indices()
 *  @param indices_count  number of entries in indices array
 *  @param indices an array with indices into "main" table-like vector set
 *  @param vec_count number of vectors in "main" table set
 *  @param vec an array of vectors in "main" table set
 *  @param vec_data an array of pointers to memory mapped areas those "main" vectors derive from. This allows computing hash from vectors drawn from different MVL files
 *  @param hm a previosly computed HASH_MAP of "main" table set
 *  @param key_last this is an output array of size key_indices_count that describes stretches of matches with indentical "key" rows. Thus for "key" row i, the corresponding stretch is key_last[i-1] to key_last[i]-1
 *  @param pairs_size the size of allocated key_match_indices and match_indices arrays. This value can be computed with mvl_hash_match_count().
 *  @param key_match_indices an array of "key" indices from each pair
 *  @param match_indices an array of "main" indices from each pair
 *  @return 0 if everything went well, otherwise a negative error code
 */
int mvl_find_matches(LIBMVL_OFFSET64 key_indices_count, const LIBMVL_OFFSET64 *key_indices, LIBMVL_OFFSET64 key_vec_count, LIBMVL_VECTOR **key_vec, void **key_vec_data, LIBMVL_OFFSET64 *key_hash,
			   LIBMVL_OFFSET64 indices_count, const LIBMVL_OFFSET64 *indices, LIBMVL_OFFSET64 vec_count, LIBMVL_VECTOR **vec, void **vec_data, HASH_MAP *hm, 
			   LIBMVL_OFFSET64 *key_last, LIBMVL_OFFSET64 pairs_size, LIBMVL_OFFSET64 *key_match_indices, LIBMVL_OFFSET64 *match_indices)
{
LIBMVL_OFFSET64 *hash, *hash_map, *next;
LIBMVL_OFFSET64 hash_map_size, i, k, hash_mask, N_matches;
MVL_SORT_INFO key_si, si;
MVL_SORT_UNIT key_su, su;

key_si.vec=key_vec;
key_si.data=key_vec_data;
key_si.nvec=key_vec_count;

si.vec=vec;
si.data=vec_data;
si.nvec=vec_count;

key_su.info=&key_si;
su.info=&si;

hash_map_size=hm->hash_map_size;
hash_mask=hash_map_size-1;
hash_map=hm->hash_map;
hash=hm->hash;
next=hm->next;

N_matches=0;

if(hash_map_size & hash_mask) {
	for(i=0;i<key_indices_count;i++) {
		k=hash_map[key_hash[i] % hash_map_size];
		key_su.index=key_indices[i];
		while(k!=~0LLU) {
			su.index=indices[k];
			if((hash[k]==key_hash[i])  && mvl_equals(&key_su, &su) ) {
				if(N_matches>=pairs_size)return(-1000);
				key_match_indices[N_matches]=key_indices[i];
				match_indices[N_matches]=indices[k];
				N_matches++;
				}
			k=next[k];
			}
		key_last[i]=N_matches;
		}
	} else {
	for(i=0;i<key_indices_count;i++) {
		k=hash_map[key_hash[i] & hash_mask];
		key_su.index=key_indices[i];
		while(k!=~0LLU) {
			su.index=indices[k];
			if((hash[k]==key_hash[i])  && mvl_equals(&key_su, &su) ) {
				if(N_matches>=pairs_size)return(-1000);
				key_match_indices[N_matches]=key_indices[i];
				match_indices[N_matches]=indices[k];
				N_matches++;
				}
			k=next[k];
			}
		key_last[i]=N_matches;
		}
	}
return(0);
}

/*! @brief This function transforms HASH_MAP into a list of groups. Similar to GROUP BY clause in SQL.
 * 
 * The original HASH_MAP describes groups of rows with identical hashes. However, there is a (remote) possibility of collision where different rows have the same hash. This function resolves this ambiguity.
 * After calling hm->hash_map becomes invalid, but hm->first and hm->next describe exactly identical rows 
 * 
 *  @param indices_count number of elements in indices array
 *  @param indices an array of indices used to create HASH_MAP hm
 *  @param vec_count the number of LIBMVL_VECTORS considered as columns in a table
 *  @param vec an array of pointers to LIBMVL_VECTORS considered as columns in a table
 *  @param vec_data an array of pointers to memory mapped areas those LIBMVL_VECTORs derive from. This allows computing hash from vectors drawn from different MVL 
 *  @param hm a previously computed (with mvl_compute_hash_map()) HASH_MAP
 */
void mvl_find_groups(LIBMVL_OFFSET64 indices_count, const LIBMVL_OFFSET64 *indices, LIBMVL_OFFSET64 vec_count, LIBMVL_VECTOR **vec, void **vec_data, HASH_MAP *hm)
{
LIBMVL_OFFSET64 *hash, *tmp, *next;
LIBMVL_OFFSET64 i, j, l, m, k, group_count, first_count, a;
MVL_SORT_INFO si;
MVL_SORT_UNIT su1, su2;

si.vec=vec;
si.data=vec_data;
si.nvec=vec_count;

su1.info=&si;
su2.info=&si;

tmp=hm->hash_map;
hash=hm->hash;
next=hm->next;

group_count=hm->first_count;
first_count=hm->first_count;

for(i=0;i<first_count;i++) {
	k=hm->first[i];
	j=0;
	while(k!=~0LLU) {
		tmp[j]=k;
		j++;
		k=next[k];
		}
	while(j>1) {
		m=j-1;
		l=1;
		su1.index=indices[tmp[0]];
		while(l<=m) {
			su2.index=indices[tmp[l]];
			if(hash[tmp[0]]!=hash[tmp[l]] || !mvl_equals(&su1, &su2)) {
				if(l<m) {
					a=tmp[m];
					tmp[m]=tmp[l];
					tmp[l]=a;
					}
				m--;
				} else l++;
			}
		next[tmp[0]]=~0LLU;
		for(m=1;m<l;m++)next[tmp[m]]=tmp[m-1];
		if(l==j) {
			hm->first[i]=tmp[l-1];
			break;
			} else {
			hm->first[group_count]=tmp[l-1];
			group_count++;
			memmove(tmp, &(tmp[l]), (j-l)*sizeof(*tmp));
			hm->first[i]=tmp[0];
			hm->next[tmp[0]]=~0LLU;
			j-=l;
			}
		}
	
	}
hm->first_count=group_count;
}


/*! @brief Increase storage of previously allocated partition
 * 
 *  @param el Partition structure
 *  @param nelem Make sure it can contain at least that many elements
 */
void mvl_extend_partition(LIBMVL_PARTITION *el, LIBMVL_OFFSET64 nelem)
{
LIBMVL_OFFSET64 *p, new_size;
new_size=2*el->size+nelem;

p=do_malloc(new_size, sizeof(*p));
if(el->count>0)	memcpy(p, el->offset, el->count*sizeof(*p));
if(el->size>0)free(el->offset);
el->offset=p;
el->size=new_size;
}

/*! @brief Compute list of extents describing stretches of data with identical values
 *  @param el pointer to previously allocated LIBMVL_PARTITION structure
 *  @param count Number of vectors in vec
 *  @param vec Array of vectors with identical number of elements
 *  @param data Mapped data areas (needed to compare strings)
 */
void mvl_find_repeats(LIBMVL_PARTITION *el, LIBMVL_OFFSET64 count, LIBMVL_VECTOR **vec, void **data)
{
LIBMVL_OFFSET64 N;
MVL_SORT_INFO info;
MVL_SORT_UNIT a, b;

if(count<1)return;

if(el->count>=el->size)
	mvl_extend_partition(el, 1024);

N=mvl_vector_length(vec[0]);
if(mvl_vector_type(vec[0])==LIBMVL_PACKED_LIST64)N--;

for(LIBMVL_OFFSET64 i=1;i<count;i++) {
	if(mvl_vector_type(vec[i])==LIBMVL_PACKED_LIST64) {
		if(mvl_vector_length(vec[i])!=N+1) {
			return;
			}
		} else {
		if(mvl_vector_length(vec[i])!=N) {
			return;
			}
		}
	}

info.vec=vec;
info.data=data;
info.nvec=count;

a.info=&info;
a.index=0;
b.info=&info;

for(LIBMVL_OFFSET64 i=1; i<N;i++) {
	b.index=i;
	if(mvl_equals(&a, &b))continue;
	
	if(el->count>=el->size)mvl_extend_partition(el, 0);
	
	el->offset[el->count]=a.index;
	el->count++;
	a.index=i;
	}
	
if(el->count+1>=el->size)mvl_extend_partition(el, 0);
el->offset[el->count]=a.index;
el->count++;
el->offset[el->count]=N;
el->count++;
}

/*! @brief Initialize freshly allocated partition structure.
 *  @param el a pointer to LIBMVL_PARTITION structure
 */
void mvl_init_partitiion(LIBMVL_PARTITION *el)
{
memset(el, 0, sizeof(*el));
}

/*! @brief free arrays of previously allocated partition. 
 *   This function does not free the structure itself.
 *  @param el a pointer to LIBMVL_PARTITION structure
 */
void mvl_free_partition_arrays(LIBMVL_PARTITION *el)
{
if(el->size>0) {
	free(el->offset);
	}
el->offset=NULL;
el->size=0;
}

/*! @brief Initialize freshly allocated partition structure.
 *  @param el a pointer to LIBMVL_PARTITION structure
 */
void mvl_init_extent_list(LIBMVL_EXTENT_LIST *el)
{
memset(el, 0, sizeof(*el));
el->size=LIBMVL_EXTENT_INLINE_SIZE;
el->start=el->start_inline;
el->stop=el->stop_inline;
}

/*! @brief free arrays of previously allocated partition. 
 *   This function does not free the structure itself.
 *  @param el a pointer to LIBMVL_PARTITION structure
 */
void mvl_free_extent_list_arrays(LIBMVL_EXTENT_LIST *el)
{
if(el->size>LIBMVL_EXTENT_INLINE_SIZE) {
	free(el->start);
	free(el->stop);
	}
el->start=NULL;
el->stop=NULL;
el->size=0;
}

/*! @brief Increase storage of previously allocated extent list
 * 
 *  @param el extent list structure
 *  @param nelem Make sure it can contain at least that many elements
 */
void mvl_extend_extent_list(LIBMVL_EXTENT_LIST *el, LIBMVL_OFFSET64 nelem)
{
LIBMVL_OFFSET64 *p;
LIBMVL_OFFSET64 new_size;

new_size=2*el->size+nelem;

p=do_malloc(new_size, sizeof(*p));
if(el->count>0)	memcpy(p, el->start, el->count*sizeof(*p));
if(el->size>LIBMVL_EXTENT_INLINE_SIZE) {
	free(el->start);
	}
el->start=p;

p=do_malloc(new_size, sizeof(*p));
if(el->count>0)	memcpy(p, el->stop, el->count*sizeof(*p));
if(el->size>LIBMVL_EXTENT_INLINE_SIZE) {
	free(el->stop);
	}
el->stop=p;

el->size=new_size;
}


/*! @brief Initialize freshly allocated extent list  structure.
 *  @param ei a pointer to LIBMVL_EXTENT_INDEX structure
 */
void mvl_init_extent_index(LIBMVL_EXTENT_INDEX *ei)
{
memset(ei, 0, sizeof(*ei));
mvl_init_partitiion(&(ei->partition));
}

/*! @brief free arrays of previously allocated extent list. 
 *   This function does not free the structure itself.
 *  @param ei a pointer to LIBMVL_EXTENT_INDEX structure
 */
void mvl_free_extent_index_arrays(LIBMVL_EXTENT_INDEX *ei)
{
mvl_free_partition_arrays(&(ei->partition));

if(ei->hash_map.flags & MVL_FLAG_OWN_FIRST)
	free(ei->hash_map.first);

if(ei->hash_map.flags & MVL_FLAG_OWN_HASH)
	free(ei->hash_map.hash);

if(ei->hash_map.flags & MVL_FLAG_OWN_NEXT)
	free(ei->hash_map.next);

if(ei->hash_map.flags & MVL_FLAG_OWN_HASH_MAP)
	free(ei->hash_map.hash_map);

if(ei->hash_map.flags & MVL_FLAG_OWN_VEC_TYPES)
	free(ei->hash_map.vec_types);

ei->hash_map.flags=0;
ei->hash_map.hash_size=0;
ei->hash_map.hash_map_size=0;
ei->hash_map.vec_count=0;
}


/*! @brief Compute an extent index.
 * 
 *  @param ei a pointer to extent index structure
 *  @param count the number of LIBMVL_VECTORS considered as columns in a table
 *  @param vec an array of pointers to LIBMVL_VECTORS considered as columns in a table
 *  @param data an array of pointers to memory mapped areas those LIBMVL_VECTORs derive from. This allows computing hash from vectors drawn from different MVL 
 *  @return an integer error code, or 0 on success
 */
int mvl_compute_extent_index(LIBMVL_EXTENT_INDEX *ei, LIBMVL_OFFSET64 count, LIBMVL_VECTOR **vec, void **data)
{
int err;
ei->partition.count=0;
mvl_find_repeats(&(ei->partition), count, vec, data);

ei->hash_map.hash_count=ei->partition.count-1;

if(ei->hash_map.hash_size<ei->hash_map.hash_count || 
	((ei->hash_map.flags  & (MVL_FLAG_OWN_HASH | MVL_FLAG_OWN_FIRST | MVL_FLAG_OWN_NEXT))!=(MVL_FLAG_OWN_HASH | MVL_FLAG_OWN_FIRST | MVL_FLAG_OWN_NEXT))) {
	if(ei->hash_map.flags & MVL_FLAG_OWN_HASH)
		free(ei->hash_map.hash);
	if(ei->hash_map.flags & MVL_FLAG_OWN_FIRST)
		free(ei->hash_map.first);
	if(ei->hash_map.flags & MVL_FLAG_OWN_NEXT)
		free(ei->hash_map.next);
	
	ei->hash_map.flags|=MVL_FLAG_OWN_HASH | MVL_FLAG_OWN_FIRST | MVL_FLAG_OWN_NEXT;
	ei->hash_map.hash_size=ei->hash_map.hash_count;
	ei->hash_map.hash=do_malloc(ei->hash_map.hash_size, sizeof(*ei->hash_map.hash));
	ei->hash_map.first=do_malloc(ei->hash_map.hash_size, sizeof(*ei->hash_map.first));
	ei->hash_map.next=do_malloc(ei->hash_map.hash_size, sizeof(*ei->hash_map.next));
	}
if(ei->hash_map.hash_map_size<ei->hash_map.hash_count || !(ei->hash_map.flags & MVL_FLAG_OWN_HASH_MAP)) {
	if(ei->hash_map.flags & MVL_FLAG_OWN_HASH_MAP) {
		free(ei->hash_map.hash_map);
		}
	ei->hash_map.flags|=MVL_FLAG_OWN_HASH_MAP;
	ei->hash_map.hash_map_size=mvl_compute_hash_map_size(ei->hash_map.hash_count);
	ei->hash_map.hash_map=do_malloc(ei->hash_map.hash_map_size, sizeof(*ei->hash_map.hash_map));
	}

if((err=mvl_hash_indices(ei->hash_map.hash_count, ei->partition.offset, ei->hash_map.hash, count, vec, data, LIBMVL_COMPLETE_HASH))!=0)return(err);
   
if(ei->hash_map.flags & MVL_FLAG_OWN_VEC_TYPES)
	free(ei->hash_map.vec_types);

ei->hash_map.flags|=MVL_FLAG_OWN_VEC_TYPES;
ei->hash_map.vec_count=count;
ei->hash_map.vec_types=do_malloc(count, sizeof(*ei->hash_map.vec_types));

for(LIBMVL_OFFSET64 i=0;i<count;i++)ei->hash_map.vec_types[i]=mvl_vector_type(vec[i]);

mvl_compute_hash_map(&(ei->hash_map));
return(0);
}

/*! @brief Write extent index to MVL file 
 * 
 */
LIBMVL_OFFSET64 mvl_write_extent_index(LIBMVL_CONTEXT *ctx, LIBMVL_EXTENT_INDEX *ei)
{
LIBMVL_NAMED_LIST *L;
LIBMVL_OFFSET64 offset;
L=mvl_create_named_list(5);

mvl_add_list_entry(L, -1, "index_type", MVL_WVEC(ctx, LIBMVL_VECTOR_INT32, MVL_EXTENT_INDEX));

mvl_add_list_entry(L, -1, "partition", mvl_write_vector(ctx, LIBMVL_VECTOR_OFFSET64, ei->partition.count, ei->partition.offset, LIBMVL_NO_METADATA));
mvl_add_list_entry(L, -1, "hash", mvl_write_vector(ctx, LIBMVL_VECTOR_OFFSET64, ei->hash_map.hash_count, ei->hash_map.hash, LIBMVL_NO_METADATA));
//mvl_add_list_entry(L, -1, "first", mvl_write_vector(ctx, LIBMVL_VECTOR_OFFSET64, ei->hash_map.first_count, ei->hash_map.first, LIBMVL_NO_METADATA));
mvl_add_list_entry(L, -1, "next", mvl_write_vector(ctx, LIBMVL_VECTOR_OFFSET64, ei->hash_map.hash_count, ei->hash_map.next, LIBMVL_NO_METADATA));
mvl_add_list_entry(L, -1, "hash_map", mvl_write_vector(ctx, LIBMVL_VECTOR_OFFSET64, ei->hash_map.hash_map_size, ei->hash_map.hash_map, LIBMVL_NO_METADATA));
mvl_add_list_entry(L, -1, "vec_types", mvl_write_vector(ctx, LIBMVL_VECTOR_INT32, ei->hash_map.vec_count, ei->hash_map.vec_types, LIBMVL_NO_METADATA));
offset=mvl_write_named_list2(ctx, L, "MVL_INDEX");
mvl_free_named_list(L);
return(offset);
}

/*!  @brief Load extent index from memory mapped MVL file
 */
int mvl_load_extent_index(LIBMVL_CONTEXT *ctx, void *data, LIBMVL_OFFSET64 data_size, LIBMVL_OFFSET64 offset, LIBMVL_EXTENT_INDEX *ei)
{
LIBMVL_NAMED_LIST *L;
LIBMVL_VECTOR *vec;
L=mvl_read_named_list(ctx, data, data_size, offset);

mvl_free_extent_index_arrays(ei);
ei->partition.count=0;
ei->hash_map.hash_count=0;
ei->hash_map.first_count=0;

if(L==NULL) {
	ei->partition.count=0;
	ei->hash_map.hash_count=0;
	ei->hash_map.first_count=0;
	return(LIBMVL_ERR_INVALID_EXTENT_INDEX);
	}
	
vec=mvl_validated_vector_from_offset(data, data_size, mvl_find_list_entry(L, -1, "partition"));
if(vec==NULL) {
	ei->partition.count=0;
	ei->hash_map.hash_count=0;
	ei->hash_map.first_count=0;
	return(LIBMVL_ERR_INVALID_EXTENT_INDEX);
	}

ei->partition.size=0;
ei->partition.offset=mvl_vector_data_offset(vec);
ei->partition.count=mvl_vector_length(vec);

vec=mvl_validated_vector_from_offset(data, data_size, mvl_find_list_entry(L, -1, "hash"));
if(vec==NULL) {
	ei->partition.count=0;
	ei->hash_map.hash_count=0;
	ei->hash_map.first_count=0;
	return(LIBMVL_ERR_INVALID_EXTENT_INDEX);
	}
ei->hash_map.hash_size=0;
ei->hash_map.hash_count=mvl_vector_length(vec);
ei->hash_map.hash=mvl_vector_data_offset(vec);

#if 0
vec=mvl_vector_from_offset(data, mvl_find_list_entry(L, -1, "first"));
if(vec==NULL) {
	ei->partition.count=0;
	ei->hash_map.hash_count=0;
	ei->hash_map.first_count=0;
	return(LIBMVL_ERR_INVALID_EXTENT_INDEX);
	}
ei->hash_map.first=mvl_vector_data_offset(vec);
ei->hash_map.first_count=mvl_vector_length(vec);
#else
ei->hash_map.first=NULL;
ei->hash_map.first_count=0;
#endif

vec=mvl_validated_vector_from_offset(data, data_size, mvl_find_list_entry(L, -1, "next"));
if(vec==NULL || mvl_vector_length(vec)!=ei->hash_map.hash_count) {
	ei->partition.count=0;
	ei->hash_map.hash_count=0;
	ei->hash_map.first_count=0;
	return(LIBMVL_ERR_INVALID_EXTENT_INDEX);
	}
ei->hash_map.next=mvl_vector_data_offset(vec);

vec=mvl_validated_vector_from_offset(data, data_size, mvl_find_list_entry(L, -1, "hash_map"));
if(vec==NULL) {
	ei->partition.count=0;
	ei->hash_map.hash_count=0;
	ei->hash_map.first_count=0;
	return(LIBMVL_ERR_INVALID_EXTENT_INDEX);
	}
ei->hash_map.hash_map_size=mvl_vector_length(vec);
ei->hash_map.hash_map=mvl_vector_data_offset(vec);

vec=mvl_validated_vector_from_offset(data, data_size, mvl_find_list_entry(L, -1, "vec_types"));
if(vec==NULL) {
	ei->partition.count=0;
	ei->hash_map.hash_count=0;
	ei->hash_map.first_count=0;
	return(LIBMVL_ERR_INVALID_EXTENT_INDEX);
	}
ei->hash_map.vec_count=mvl_vector_length(vec);
ei->hash_map.vec_types=mvl_vector_data_int32(vec);
mvl_free_named_list(L);

return(0);
}

/*! @brief Compute vector statistics, such as a bounding box
 *  @param vec a pointer to LIBMVL_VECTOR
 *  @param stats a pointer to previously allocated LIBMVL_VEC_STATS structure
 */
void mvl_compute_vec_stats(const LIBMVL_VECTOR *vec, LIBMVL_VEC_STATS *stats)
{
if(mvl_vector_length(vec)<1) {
	stats->max=-1;
	stats->min=1;
	stats->center=0.0;
	stats->scale=0.0;
	stats->nrepeat=0;
	stats->average_repeat_length=0.0;
	return;
	}
switch(mvl_vector_type(vec)) {
	case LIBMVL_VECTOR_DOUBLE: {
		double a0, a1, b;
		double *pd=mvl_vector_data_double(vec);
		LIBMVL_OFFSET64 nrepeat;
		double prev;
		a0=pd[0];
		a1=a0;
		prev=a0;
		nrepeat=0;
		for(LIBMVL_OFFSET64 i=1;i<mvl_vector_length(vec);i++) {
			b=pd[i];
			if(b>a1)a1=b;
			if(b<a0)a0=b;
			if(b!=prev) {
				nrepeat++;
				prev=b;
				}
			}
		nrepeat++;
		stats->nrepeat=nrepeat;
		stats->average_repeat_length=(1.0*mvl_vector_length(vec))/nrepeat;
		stats->max=a1;
		stats->min=a0;
		stats->center=(a0+a1)*0.5;
		if(a1>a0)
			stats->scale=2/(a1-a0);
			else
			stats->scale=0.0;
		break;
		}
	case LIBMVL_VECTOR_FLOAT: {
		float a0, a1, b;
		float *pd=mvl_vector_data_float(vec);
		LIBMVL_OFFSET64 nrepeat;
		float prev;
		a0=pd[0];
		a1=a0;
		prev=a0;
		nrepeat=0;
		for(LIBMVL_OFFSET64 i=1;i<mvl_vector_length(vec);i++) {
			b=pd[i];
			if(b>a1)a1=b;
			if(b<a0)a0=b;
			if(b!=prev) {
				nrepeat++;
				prev=b;
				}
			}
		nrepeat++;
		stats->nrepeat=nrepeat;
		stats->average_repeat_length=(1.0*mvl_vector_length(vec))/nrepeat;
		stats->max=a1;
		stats->min=a0;
		stats->center=(a0+a1)*0.5;
		if(a1>a0)
			stats->scale=2/(a1-a0);
			else
			stats->scale=0.0;
		break;
		}
	case LIBMVL_VECTOR_INT32: {
		int a0, a1, b;
		int *pd=mvl_vector_data_int32(vec);
		LIBMVL_OFFSET64 nrepeat;
		int prev;
		a0=pd[0];
		a1=a0;
		prev=a0;
		nrepeat=0;
		for(LIBMVL_OFFSET64 i=1;i<mvl_vector_length(vec);i++) {
			b=pd[i];
			if(b>a1)a1=b;
			if(b<a0)a0=b;
			if(b!=prev) {
				nrepeat++;
				prev=b;
				}
			}
		nrepeat++;
		stats->nrepeat=nrepeat;
		stats->average_repeat_length=(1.0*mvl_vector_length(vec))/nrepeat;
		stats->max=a1;
		stats->min=a0;
		stats->center=(a0*1.0+a1*1.0)*0.5;
		if(a1>a0)
			stats->scale=2/(a1-a0);
			else
			stats->scale=0.0;
		break;
		}
	case LIBMVL_VECTOR_INT64: {
		long long int a0, a1, b;
		long long int *pd=mvl_vector_data_int64(vec);
		LIBMVL_OFFSET64 nrepeat;
		long long int prev;
		a0=pd[0];
		a1=a0;
		prev=a0;
		nrepeat=0;
		for(LIBMVL_OFFSET64 i=1;i<mvl_vector_length(vec);i++) {
			b=pd[i];
			if(b>a1)a1=b;
			if(b<a0)a0=b;
			if(b!=prev) {
				nrepeat++;
				prev=b;
				}
			}
		nrepeat++;
		stats->nrepeat=nrepeat;
		stats->average_repeat_length=(1.0*mvl_vector_length(vec))/nrepeat;
		stats->max=a1;
		stats->min=a0;
		stats->center=(a0*1.0+a1*1.0)*0.5;
		if(a1>a0)
			stats->scale=2/(a1-a0);
			else
			stats->scale=0.0;
		break;
		}
	default:
		stats->max=-1;
		stats->min=1;
		stats->center=0.0;
		stats->scale=0.0;
		stats->nrepeat=0;
		stats->average_repeat_length=0.0;
	}
}

/*!  @brief normalize vector
 * 
 *   This function converts numeric vectors into a normalized double precision entries. Indices i0 and i1 specify the stretch of indices to normalize. This facilitates processing of very long vectors in pieces.
 *   @param vec a pointer to LIBMVL_VECTOR
 *   @param stats previously allocated LIBMVL_VEC_STATS structure
 *   @param i0 start index of stretch to process
 *   @param i1 stop index of stretch to process
 *   @param out array of normalized entries of size i1-i0. First entry corresponds to index i0
 */
void mvl_normalize_vector(const LIBMVL_VECTOR *vec, const LIBMVL_VEC_STATS *stats, LIBMVL_OFFSET64 i0, LIBMVL_OFFSET64 i1, double *out)
{
double scale, center;
scale=0.5*stats->scale;
center=1.5-stats->center*scale;
if(i0>mvl_vector_length(vec))return;
if(i1>mvl_vector_length(vec)) {
	LIBMVL_OFFSET64 i=mvl_vector_length(vec);
	if(i<i0)i=i0;
	for(;i<i1;i++)out[i-i0]=0.0;
	i1=mvl_vector_length(vec);
	}
if(i0>=i1)return;

switch(mvl_vector_type(vec)) {
	case LIBMVL_VECTOR_DOUBLE: {
		double *pd=mvl_vector_data_double(vec);
		for(LIBMVL_OFFSET64 i=i0;i<i1;i++) {
			out[i-i0]=pd[i]*scale+center;
			}
		break;
		}
	case LIBMVL_VECTOR_FLOAT: {
		float *pd=mvl_vector_data_float(vec);
		for(LIBMVL_OFFSET64 i=i0;i<i1;i++) {
			out[i-i0]=pd[i]*scale+center;
			}
		break;
		}
	case LIBMVL_VECTOR_INT32: {
		int *pd=mvl_vector_data_int32(vec);
		for(LIBMVL_OFFSET64 i=i0;i<i1;i++) {
			out[i-i0]=pd[i]*scale+center;
			}
		break;
		}
	case LIBMVL_VECTOR_INT64: {
		long long int *pd=mvl_vector_data_int64(vec);
		for(LIBMVL_OFFSET64 i=i0;i<i1;i++) {
			out[i-i0]=pd[i]*scale+center;
			}
		break;
		}
	default:
		for(LIBMVL_OFFSET64 i=i0;i<i1;i++)out[i-i0]=0.0;
	}
}
