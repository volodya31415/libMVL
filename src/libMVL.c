
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

LIBMVL_OFFSET64 mvl_write_concat_vectors(LIBMVL_CONTEXT *ctx, int type, long nvec, long *lengths, void **data, LIBMVL_OFFSET64 metadata)
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

LIBMVL_OFFSET64 mvl_write_packed_list(LIBMVL_CONTEXT *ctx, long count, long *str_size,  char **str, LIBMVL_OFFSET64 metadata)
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
	
ctx->dir_free=0;

ctx->directory_offset=offset;
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
char *p, *d;
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
return(p->u.offset[i]);
}

LIBMVL_OFFSET64 mvl_directory_entry(void *data, int i)
{
LIBMVL_VECTOR *p=(LIBMVL_VECTOR *)data;
return(p->u.offset[i+(p->header.length>>1)]);
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
	ctx->directory[i].offset=dir->u.offset[i+ctx->dir_free];
	a=(LIBMVL_VECTOR *)&(((unsigned char *)data)[dir->u.offset[i]]);
	ctx->directory[i].tag=memndup(a->u.b, a->header.length);
	}
}
