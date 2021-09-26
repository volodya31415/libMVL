
/* (c) Vladimir Dergachev 2019-2021 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
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

static void *do_malloc(long a, long b)
{
void *r;
int i=0;
if(a<1)a=1;
if(b<1)b=1;
r=malloc(a*b);
while(r==NULL){
#ifdef USING_R
	Rprintf("libMVL: Could not allocate %ld chunks of %ld bytes each (%ld bytes total)\n",a,b,a*b);
#else
	fprintf(stderr,"libMVL: Could not allocate %ld chunks of %ld bytes each (%ld bytes total)\n",a,b,a*b);
#endif
//	if(i>args_info.memory_allocation_retries_arg)exit(-1);
	sleep(10);
	r=malloc(a*b);
	i++;
	}
//if(a*b>10e6)madvise(r, a*b, MADV_HUGEPAGE);
return r;
}

static inline char *memndup(const char *s, int len)
{
char *p;
int i;
p=do_malloc(len+1, 1);
for(i=0;i<len;i++)p[i]=s[i];
p[len]=0;
return(p);
}

LIBMVL_CONTEXT *mvl_create_context(void)
{
LIBMVL_CONTEXT *ctx;
//ctx=calloc(1, sizeof(*ctx));
ctx=do_malloc(1, sizeof(*ctx));
if(ctx==NULL)return(ctx);

ctx->error=0;
ctx->abort_on_error=1;
ctx->alignment=32;

ctx->dir_size=100;
ctx->dir_free=0;

ctx->directory=do_malloc(ctx->dir_size, sizeof(*ctx->directory));
ctx->directory_offset=-1;

ctx->character_class_offset=0;

ctx->cached_strings=mvl_create_named_list(32);

return(ctx);
}

void mvl_free_context(LIBMVL_CONTEXT *ctx)
{
for(LIBMVL_OFFSET64 i=0;i<ctx->dir_free;i++)
	free(ctx->directory[i].tag);
free(ctx->directory);
mvl_free_named_list(ctx->cached_strings);
free(ctx);
}

void mvl_set_error(LIBMVL_CONTEXT *ctx, int error)
{
ctx->error=error;
if(ctx->abort_on_error) {
#ifdef USING_R
	Rprintf("*** ERROR: libMVL code %d\n", error);
#else
	fprintf(stderr, "*** ERROR: libMVL code %d\n", error);
	exit(-1);
#endif
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
cur=ftello(ctx->f);
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
ctx->tmp_postamble.type=LIBMVL_VECTOR_POSTAMBLE;
mvl_write(ctx, sizeof(ctx->tmp_postamble), &ctx->tmp_postamble);
}

LIBMVL_OFFSET64 mvl_write_vector(LIBMVL_CONTEXT *ctx, int type, long length, const void *data, LIBMVL_OFFSET64 metadata)
{
LIBMVL_OFFSET64 byte_length;
int padding;
unsigned char *zeros;
LIBMVL_OFFSET64 offset;

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

offset=ftello(ctx->f);

if((long long)offset<0) {
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

LIBMVL_OFFSET64 mvl_start_write_vector(LIBMVL_CONTEXT *ctx, int type, long expected_length, long length, const void *data, LIBMVL_OFFSET64 metadata)
{
LIBMVL_OFFSET64 byte_length, total_byte_length;
int padding;
unsigned char *zeros;
LIBMVL_OFFSET64 offset;

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

offset=ftello(ctx->f);

if((long long)offset<0) {
	perror("mvl_write_vector");
	mvl_set_error(ctx, LIBMVL_ERR_FTELL);
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

void mvl_rewrite_vector(LIBMVL_CONTEXT *ctx, int type, LIBMVL_OFFSET64 base_offset, LIBMVL_OFFSET64 idx, long length, const void *data)
{
LIBMVL_OFFSET64 byte_length, elt_size;

elt_size=mvl_element_size(type);
byte_length=length*elt_size;

if(byte_length>0)mvl_rewrite(ctx, base_offset+elt_size*idx+sizeof(ctx->tmp_vh), byte_length, data);
}

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
				pb[i]=mvl_vector_data(vec).b[indices[i+i_start]];
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
				pi[i]=mvl_vector_data(vec).i[indices[i+i_start]];
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
				pi[i]=mvl_vector_data(vec).i64[indices[i+i_start]];
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
				pf[i]=mvl_vector_data(vec).f[indices[i+i_start]];
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
				pd[i]=mvl_vector_data(vec).d[indices[i+i_start]];
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

LIBMVL_OFFSET64 mvl_write_concat_vectors(LIBMVL_CONTEXT *ctx, int type, long nvec, const long *lengths, void **data, LIBMVL_OFFSET64 metadata)
{
LIBMVL_OFFSET64 byte_length, length;
int padding, item_size;
unsigned char *zeros;
LIBMVL_OFFSET64 offset;
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

offset=ftello(ctx->f);

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

/* Writes a single C string. In particular, this is handy for providing metadata tags */
/* length can be specified as -1 to be computed automatically */
LIBMVL_OFFSET64 mvl_write_string(LIBMVL_CONTEXT *ctx, long length, const char *data, LIBMVL_OFFSET64 metadata)
{
if(length<0)length=strlen(data);
return(mvl_write_vector(ctx, LIBMVL_VECTOR_CSTRING, length, data, metadata));
}

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

LIBMVL_OFFSET64 mvl_write_packed_list(LIBMVL_CONTEXT *ctx, long count, const long *str_size,  char **str, LIBMVL_OFFSET64 metadata)
{
LIBMVL_OFFSET64 *ofsv, ofs1, ofs2, len1;
long *str_size2;
long i;
ofsv=do_malloc(count+1, sizeof(*ofsv));
str_size2=do_malloc(count, sizeof(*str_size2));

len1=0;
for(i=0;i<count;i++) {
	if((str_size==NULL) || (str_size[i]<0)) {
		str_size2[i]=strlen(str[i]);
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

void mvl_add_directory_entry(LIBMVL_CONTEXT *ctx, LIBMVL_OFFSET64 offset, const char *tag)
{
LIBMVL_DIRECTORY_ENTRY *p;
if(ctx->dir_free>=ctx->dir_size) {
	ctx->dir_size+=ctx->dir_size+10;
	
	p=do_malloc(ctx->dir_size, sizeof(*p));
	if(ctx->dir_free>0)memcpy(p, ctx->directory, ctx->dir_free*sizeof(*p));
	free(ctx->directory);
	ctx->directory=p;
	}
//fprintf(stderr, "Adding entry %d \"%s\"=0x%016x\n", ctx->dir_free, tag, offset);
ctx->directory[ctx->dir_free].offset=offset;
ctx->directory[ctx->dir_free].tag=strdup(tag);
ctx->dir_free++;
}

void mvl_add_directory_entry_n(LIBMVL_CONTEXT *ctx, LIBMVL_OFFSET64 offset, const char *tag, LIBMVL_OFFSET64 tag_size)
{
LIBMVL_DIRECTORY_ENTRY *p;
if(ctx->dir_free>=ctx->dir_size) {
	ctx->dir_size+=ctx->dir_size+10;
	
	p=do_malloc(ctx->dir_size, sizeof(*p));
	if(ctx->dir_free>0)memcpy(p, ctx->directory, ctx->dir_free*sizeof(*p));
	free(ctx->directory);
	ctx->directory=p;
	}
ctx->directory[ctx->dir_free].offset=offset;
ctx->directory[ctx->dir_free].tag=memndup(tag, tag_size);
ctx->dir_free++;
}

LIBMVL_OFFSET64 mvl_write_directory(LIBMVL_CONTEXT *ctx)
{
LIBMVL_OFFSET64 *p;
LIBMVL_OFFSET64 offset;
int i;


if(ctx->dir_free<1) {
	mvl_set_error(ctx, LIBMVL_ERR_EMPTY_DIRECTORY);
	return(0);
	}

p=do_malloc(ctx->dir_free*2, sizeof(*p));
for(i=0;i<ctx->dir_free;i++) {
	p[i]=mvl_write_vector(ctx, LIBMVL_VECTOR_UINT8,  strlen(ctx->directory[i].tag), ctx->directory[i].tag, LIBMVL_NO_METADATA);
	p[i+ctx->dir_free]=ctx->directory[i].offset;
	}

	
offset=ftello(ctx->f);

if((long long)offset<0) {
	perror("mvl_write_directory");
	mvl_set_error(ctx, LIBMVL_ERR_FTELL);
	}

mvl_write_vector(ctx, LIBMVL_VECTOR_OFFSET64, 2*ctx->dir_free, p, LIBMVL_NO_METADATA);

ctx->directory_offset=offset;
free(p);
return(offset);
}

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
L->hash_mult=217596121;

return(L);
}

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
k=L->free;
L->free++;
L->offset[k]=offset;
if(tag_length<0)tag_length=strlen(tag);
L->tag_length[k]=tag_length;
L->tag[k]=memndup(tag, tag_length);

if(L->hash_size>0) {
	/* TODO: automatically add to hash table if present */
	}
return(k);
}

LIBMVL_OFFSET64 mvl_find_list_entry(LIBMVL_NAMED_LIST *L, long tag_length, const char *tag)
{
long i, tl;
if(L->hash_size>0) {
	/* TODO: use has table */
	}
tl=tag_length;
if(tl<0)tl=strlen(tag);
for(i=0;i<L->free;i++) {
	if(L->tag_length[i]!=tl)continue;
	if(!memcmp(L->tag[i], tag, tl)) {
		return(L->offset[i]);
		}
	}
return(LIBMVL_NULL_OFFSET);
}


LIBMVL_NAMED_LIST *mvl_create_R_attributes_list(LIBMVL_CONTEXT *ctx, const char *R_class)
{
LIBMVL_NAMED_LIST *L;
L=mvl_create_named_list(-1);
mvl_add_list_entry(L, -1, "MVL_LAYOUT", mvl_write_cached_string(ctx, -1, "R"));
mvl_add_list_entry(L, -1, "class", mvl_write_cached_string(ctx, -1, R_class));
return(L);
}

LIBMVL_OFFSET64 mvl_write_attributes_list(LIBMVL_CONTEXT *ctx, LIBMVL_NAMED_LIST *L)
{
LIBMVL_OFFSET64 *offsets, attr_offset;
long i;
offsets=do_malloc(2*L->free, sizeof(*offsets));

for(i=0;i<L->free;i++) {
	offsets[i]=mvl_write_cached_string(ctx, L->tag_length[i], L->tag[i]);
	}
memcpy(&(offsets[L->free]), L->offset, L->free*sizeof(*offsets));

attr_offset=mvl_write_vector(ctx, LIBMVL_VECTOR_OFFSET64, 2*L->free, offsets, LIBMVL_NO_METADATA);

free(offsets);

return(attr_offset);
}

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
LIBMVL_NAMED_LIST *mvl_read_attributes_list(LIBMVL_CONTEXT *ctx, const void *data, LIBMVL_OFFSET64 metadata_offset)
{
LIBMVL_NAMED_LIST *L;
long i, nattr;
char *p, *d;

if(metadata_offset==LIBMVL_NO_METADATA)return(NULL);

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
	mvl_add_list_entry(L, 
		mvl_vector_length(&(d[mvl_vector_data(p).offset[i]])), 
		mvl_vector_data(&(d[mvl_vector_data(p).offset[i]])).b, 
		mvl_vector_data(p).offset[i+nattr]);
	}

return(L);
}

/* This is meant to operate on memory mapped files */
LIBMVL_NAMED_LIST *mvl_read_named_list(LIBMVL_CONTEXT *ctx, const void *data, LIBMVL_OFFSET64 offset)
{
LIBMVL_NAMED_LIST *L, *Lattr;
char *d;
LIBMVL_OFFSET64 names_ofs, tag_ofs;
long i, nelem;

if(offset==LIBMVL_NULL_OFFSET)return(NULL);

d=(char *)data;

if(mvl_vector_type(&(d[offset]))!=LIBMVL_VECTOR_OFFSET64){
	mvl_set_error(ctx, LIBMVL_ERR_INVALID_OFFSET);
	return(NULL);
	}

Lattr=mvl_read_attributes_list(ctx, data, mvl_vector_metadata_offset(&(d[offset])));
if(Lattr==NULL)return(NULL);
names_ofs=mvl_find_list_entry(Lattr, -1, "names");

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
			tag_ofs=mvl_vector_data(&(d[names_ofs])).offset[i];
			mvl_add_list_entry(L, mvl_vector_length(&(d[tag_ofs])), mvl_vector_data(&(d[tag_ofs])).b, mvl_vector_data(&(d[offset])).offset[i]);
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
			mvl_add_list_entry(L, mvl_packed_list_get_entry_bytelength((LIBMVL_VECTOR *)&(d[names_ofs]), i), mvl_packed_list_get_entry((LIBMVL_VECTOR *)&(d[names_ofs]), d, i), mvl_vector_data(&(d[offset])).offset[i]);
			}
		break;
	default:
		mvl_free_named_list(L);
		mvl_free_named_list(Lattr);
		mvl_set_error(ctx, LIBMVL_ERR_INVALID_ATTR);
		return(NULL);
	}

mvl_free_named_list(Lattr);
return(L);
}

void mvl_open(LIBMVL_CONTEXT *ctx, FILE *f)
{
ctx->f=f;
mvl_write_preamble(ctx);
}

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

LIBMVL_OFFSET64 mvl_directory_tag(const void *data, int i)
{
LIBMVL_VECTOR *p=(LIBMVL_VECTOR *)data;
return(mvl_vector_data(p).offset[i]);
}

LIBMVL_OFFSET64 mvl_directory_entry(void *data, int i)
{
LIBMVL_VECTOR *p=(LIBMVL_VECTOR *)data;
return(mvl_vector_data(p).offset[i+(p->header.length>>1)]);
}

LIBMVL_OFFSET64 mvl_find_directory_entry(LIBMVL_CONTEXT *ctx, const char *tag)
{
int i;
for(i=ctx->dir_free-1;i>=0;i--) {
	if(!strcmp(tag, ctx->directory[i].tag))return(ctx->directory[i].offset);
	}
return(0);
}

void mvl_load_image(LIBMVL_CONTEXT *ctx, LIBMVL_OFFSET64 length, const void *data)
{
LIBMVL_PREAMBLE *pr=(LIBMVL_PREAMBLE *)data;
LIBMVL_POSTAMBLE *pa=(LIBMVL_POSTAMBLE *)&(((unsigned char *)data)[length-sizeof(LIBMVL_POSTAMBLE)]);
LIBMVL_VECTOR *dir, *a;
int i;

if(strncmp(pr->signature, LIBMVL_SIGNATURE, 4)) {
	mvl_set_error(ctx, LIBMVL_ERR_INVALID_SIGNATURE);
	return;
	}

if(pr->endianness!=LIBMVL_ENDIANNESS_FLAG) {
	mvl_set_error(ctx, LIBMVL_ERR_WRONG_ENDIANNESS);
	return;
	}
	
if(pa->type!=LIBMVL_VECTOR_POSTAMBLE) {
	mvl_set_error(ctx, LIBMVL_ERR_CORRUPT_POSTAMBLE);
	return;
	}

//fprintf(stderr, "Reading MVL directory at offset 0x%08llx\n", pa->directory);
dir=(LIBMVL_VECTOR *)&(((unsigned char *)data)[pa->directory]);

for(i=0;i<ctx->dir_free;i++) {
	free(ctx->directory[i].tag);
	ctx->directory[i].tag=NULL;
	ctx->directory[i].offset=0;
	}

ctx->dir_free=dir->header.length>>1;
//fprintf(stderr, "Reading MVL with %ld directory entries\n", ctx->dir_free);
if(ctx->dir_free >= ctx->dir_size) {
	ctx->dir_size=ctx->dir_free;
	free(ctx->directory);
	ctx->directory=do_malloc(ctx->dir_size, sizeof(*ctx->directory));
	}
	
for(i=0;i<ctx->dir_free;i++) {
	ctx->directory[i].offset=mvl_vector_data(dir).offset[i+ctx->dir_free];
	a=(LIBMVL_VECTOR *)&(((unsigned char *)data)[mvl_vector_data(dir).offset[i]]);
	ctx->directory[i].tag=memndup(mvl_vector_data(a).b, a->header.length);
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
			ad=mvl_vector_data(avec).b[ai];
			bd=mvl_vector_data(bvec).b[bi];
			if(ad!=bd)return 0;
			break;
			}
		case LIBMVL_VECTOR_INT32: {
			int ad;
			ad=mvl_vector_data(avec).i[ai];
			switch(mvl_vector_type(bvec)) {
				case LIBMVL_VECTOR_INT32: {
					int bd;
					bd=mvl_vector_data(bvec).i[bi];
					if(ad!=bd)return 0;
					break;
					}
				case LIBMVL_VECTOR_INT64: {
					long long bd;
					bd=mvl_vector_data(bvec).i64[bi];
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
			ad=mvl_vector_data(avec).i64[ai];
			switch(mvl_vector_type(bvec)) {
				case LIBMVL_VECTOR_INT32: {
					int bd;
					bd=mvl_vector_data(bvec).i[bi];
					if(ad!=bd)return 0;
					break;
					}
				case LIBMVL_VECTOR_INT64: {
					long long bd;
					bd=mvl_vector_data(bvec).i64[bi];
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
			ad=mvl_vector_data(avec).f[ai];
			switch(mvl_vector_type(bvec)) {
				case LIBMVL_VECTOR_FLOAT: {
					float bd;
					bd=mvl_vector_data(bvec).f[bi];
					if(ad!=bd)return 0;
					break;
					}
				case LIBMVL_VECTOR_DOUBLE: {
					double bd;
					bd=mvl_vector_data(bvec).d[bi];
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
			ad=mvl_vector_data(avec).d[ai];
			switch(mvl_vector_type(bvec)) {
				case LIBMVL_VECTOR_FLOAT: {
					double bd;
					bd=mvl_vector_data(bvec).f[bi];
					if(ad!=bd)return 0;
					break;
					}
				case LIBMVL_VECTOR_DOUBLE: {
					double bd;
					bd=mvl_vector_data(bvec).d[bi];
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
			ad=mvl_vector_data(avec).offset[ai];
			bd=mvl_vector_data(bvec).offset[bi];
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
			ad=mvl_vector_data(vec).b[ai];
			bd=mvl_vector_data(vec).b[bi];
			if(ad<bd)return -1;
			if(ad>bd)return 1;
			break;
			}
		case LIBMVL_VECTOR_INT32: {
			int ad, bd;
			ad=mvl_vector_data(vec).i[ai];
			bd=mvl_vector_data(vec).i[bi];
			if(ad<bd)return -1;
			if(ad>bd)return 1;
			break;
			}
		case LIBMVL_VECTOR_FLOAT: {
			float ad, bd;
			ad=mvl_vector_data(vec).f[ai];
			bd=mvl_vector_data(vec).f[bi];
			if(ad<bd)return -1;
			if(ad>bd)return 1;
			break;
			}
		case LIBMVL_VECTOR_INT64: {
			long long ad, bd;
			ad=mvl_vector_data(vec).i64[ai];
			bd=mvl_vector_data(vec).i64[bi];
			if(ad<bd)return -1;
			if(ad>bd)return 1;
			break;
			}
		case LIBMVL_VECTOR_DOUBLE: {
			double ad, bd;
			ad=mvl_vector_data(vec).d[ai];
			bd=mvl_vector_data(vec).d[bi];
			if(ad<bd)return -1;
			if(ad>bd)return 1;
			break;
			}
		case LIBMVL_VECTOR_OFFSET64: {
			LIBMVL_OFFSET64 ad, bd;
			ad=mvl_vector_data(vec).offset[ai];
			bd=mvl_vector_data(vec).offset[bi];
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
			ad=mvl_vector_data(vec).b[ai];
			bd=mvl_vector_data(vec).b[bi];
			if(ad<bd)return 1;
			if(ad>bd)return -1;
			break;
			}
		case LIBMVL_VECTOR_INT32: {
			int ad, bd;
			ad=mvl_vector_data(vec).i[ai];
			bd=mvl_vector_data(vec).i[bi];
			if(ad<bd)return 1;
			if(ad>bd)return -1;
			break;
			}
		case LIBMVL_VECTOR_FLOAT: {
			float ad, bd;
			ad=mvl_vector_data(vec).f[ai];
			bd=mvl_vector_data(vec).f[bi];
			if(ad<bd)return 1;
			if(ad>bd)return -1;
			break;
			}
		case LIBMVL_VECTOR_INT64: {
			long long ad, bd;
			ad=mvl_vector_data(vec).i64[ai];
			bd=mvl_vector_data(vec).i64[bi];
			if(ad<bd)return 1;
			if(ad>bd)return -1;
			break;
			}
		case LIBMVL_VECTOR_DOUBLE: {
			double ad, bd;
			ad=mvl_vector_data(vec).d[ai];
			bd=mvl_vector_data(vec).d[bi];
			if(ad<bd)return 1;
			if(ad>bd)return -1;
			break;
			}
		case LIBMVL_VECTOR_OFFSET64: {
			LIBMVL_OFFSET64 ad, bd;
			ad=mvl_vector_data(vec).offset[ai];
			bd=mvl_vector_data(vec).offset[bi];
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

N=mvl_vector_length(vec[0]);
//fprintf(stderr, "vec_count=%d N=%d\n", vec_count, N);
if(mvl_vector_type(vec[0])==LIBMVL_PACKED_LIST64)N--;
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

/* This function is used to compute 64 bit hash of vector values
 * array hash[] is passed in and contains the result of the computation
 * 
 * Integer indices are computed by value, so that 100 produces the same hash whether it is stored as INT32 or INT64.
 * 
 * Floats and doubles are trickier - we can guarantee that the hash of float promoted to double is the same as the hash of the original float, but not the reverse.
 */
int mvl_hash_indices(LIBMVL_OFFSET64 indices_count, const LIBMVL_OFFSET64 *indices, LIBMVL_OFFSET64 *hash, LIBMVL_OFFSET64 vec_count, LIBMVL_VECTOR **vec, void **vec_data)
{
LIBMVL_OFFSET64 i, j, N;

for(i=0;i<indices_count;i++) {
	hash[i]=MVL_SEED_HASH_VALUE;
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
				hash[i]=mvl_accumulate_hash64(hash[i], (const char *)&(mvl_vector_data(vec[j]).i[indices[i]]), 1);
				}
			break;
		case LIBMVL_VECTOR_INT32:
			for(i=0;i<indices_count;i++) {
				hash[i]=mvl_accumulate_int32_hash64(hash[i], &(mvl_vector_data(vec[j]).i[indices[i]]), 1);
				}
			break;
		case LIBMVL_VECTOR_INT64:
			for(i=0;i<indices_count;i++) {
				hash[i]=mvl_accumulate_int64_hash64(hash[i], &(mvl_vector_data(vec[j]).i64[indices[i]]), 1);
				}
			break;
		case LIBMVL_VECTOR_FLOAT:
			for(i=0;i<indices_count;i++) {
				hash[i]=mvl_accumulate_float_hash64(hash[i], &(mvl_vector_data(vec[j]).f[indices[i]]), 1);
				}
			break;
		case LIBMVL_VECTOR_DOUBLE:
			for(i=0;i<indices_count;i++) {
				hash[i]=mvl_accumulate_double_hash64(hash[i], &(mvl_vector_data(vec[j]).d[indices[i]]), 1);
				}
			break;
		case LIBMVL_VECTOR_OFFSET64: /* TODO: we might want to do something more clever here */
			for(i=0;i<indices_count;i++) {
				hash[i]=mvl_accumulate_hash64(hash[i], (const char *)&(mvl_vector_data(vec[j]).i64[indices[i]]), 8);
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
for(i=0;i<indices_count;i++) {
	hash[i]=mvl_randomize_bits64(hash[i]);
	}
	
return 0;
}

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

hm->flags=MVL_FLAG_OWN_HASH | MVL_FLAG_OWN_HASH_MAP | MVL_FLAG_OWN_FIRST | MVL_FLAG_OWN_NEXT;

return(hm);
}

void mvl_free_hash_map(HASH_MAP *hash_map)
{
if(hash_map->flags & MVL_FLAG_OWN_HASH)free(hash_map->hash);
if(hash_map->flags & MVL_FLAG_OWN_HASH_MAP)free(hash_map->hash_map);
if(hash_map->flags & MVL_FLAG_OWN_FIRST)free(hash_map->first);
if(hash_map->flags & MVL_FLAG_OWN_NEXT)free(hash_map->next);

hash_map->hash_size=0;
hash_map->hash_map_size=0;
free(hash_map);
}


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

/* Find count of matches between hashes of two sets. 
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

/* This function transforms HASH_MAP into a list of groups. 
 * After calling hm->hash_map becomes invalid, but hm->first and hm->next describe exactly identical rows 
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

void mvl_compute_vec_stats(const LIBMVL_VECTOR *vec, LIBMVL_VEC_STATS *stats)
{
if(mvl_vector_length(vec)<1) {
	stats->max=-1;
	stats->min=1;
	stats->center=0.0;
	stats->scale=0.0;
	return;
	}
switch(mvl_vector_type(vec)) {
	case LIBMVL_VECTOR_DOUBLE: {
		double a0, a1, b;
		double *pd=mvl_vector_data(vec).d;
		a0=pd[0];
		a1=a0;
		for(LIBMVL_OFFSET64 i=1;i<mvl_vector_length(vec);i++) {
			b=pd[i];
			if(b>a1)a1=b;
			if(b<a0)a0=b;
			}
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
		float *pd=mvl_vector_data(vec).f;
		a0=pd[0];
		a1=a0;
		for(LIBMVL_OFFSET64 i=1;i<mvl_vector_length(vec);i++) {
			b=pd[i];
			if(b>a1)a1=b;
			if(b<a0)a0=b;
			}
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
		int *pd=mvl_vector_data(vec).i;
		a0=pd[0];
		a1=a0;
		for(LIBMVL_OFFSET64 i=1;i<mvl_vector_length(vec);i++) {
			b=pd[i];
			if(b>a1)a1=b;
			if(b<a0)a0=b;
			}
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
		long long int *pd=mvl_vector_data(vec).i64;
		a0=pd[0];
		a1=a0;
		for(LIBMVL_OFFSET64 i=1;i<mvl_vector_length(vec);i++) {
			b=pd[i];
			if(b>a1)a1=b;
			if(b<a0)a0=b;
			}
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
	}
}

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
		double *pd=mvl_vector_data(vec).d;
		for(LIBMVL_OFFSET64 i=i0;i<i1;i++) {
			out[i-i0]=pd[i]*scale+center;
			}
		break;
		}
	case LIBMVL_VECTOR_FLOAT: {
		float *pd=mvl_vector_data(vec).f;
		for(LIBMVL_OFFSET64 i=i0;i<i1;i++) {
			out[i-i0]=pd[i]*scale+center;
			}
		break;
		}
	case LIBMVL_VECTOR_INT32: {
		int *pd=mvl_vector_data(vec).i;
		for(LIBMVL_OFFSET64 i=i0;i<i1;i++) {
			out[i-i0]=pd[i]*scale+center;
			}
		break;
		}
	case LIBMVL_VECTOR_INT64: {
		long long int *pd=mvl_vector_data(vec).i64;
		for(LIBMVL_OFFSET64 i=i0;i<i1;i++) {
			out[i-i0]=pd[i]*scale+center;
			}
		break;
		}
	default:
		for(LIBMVL_OFFSET64 i=i0;i<i1;i++)out[i-i0]=0.0;
	}
}
