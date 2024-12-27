#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "libMVL.h"


#define NDF 1000000

double ad[NDF];

float af[NDF];

int ai[NDF];

char * ac[NDF];

int main(int argc, char *argv[])
{
FILE *fout;
long i;
LIBMVL_CONTEXT *ctx;
LIBMVL_NAMED_LIST *L;
LIBMVL_OFFSET64 length;
void *data;

fout=fopen("test4.mvl", "w+");
if(fout==NULL) {
	perror("test4.mvl");
	exit(-1);
	}
	
/* Fill with test data */
for(i=0;i<NDF;i++) {
	ad[i]=(1.0*i)*(1.0*i);
	af[i]=ad[i]+10.0;
	ai[i]=i % 301;
	ac[i]= i & 0x1 ? "a" : "b";
	}
	
ctx=mvl_create_context();
mvl_open(ctx, fout);

L=mvl_create_named_list(-1);
	
mvl_add_list_entry(L, -1, "ad", mvl_write_vector(ctx, LIBMVL_VECTOR_DOUBLE, NDF, ad, LIBMVL_NO_METADATA));
mvl_add_list_entry(L, -1, "af", mvl_write_vector(ctx, LIBMVL_VECTOR_FLOAT, NDF, af, LIBMVL_NO_METADATA));
mvl_add_list_entry(L, -1, "ai", mvl_write_vector(ctx, LIBMVL_VECTOR_INT32, NDF, ai, LIBMVL_NO_METADATA));
mvl_add_list_entry(L, -1, "ac", mvl_write_packed_list(ctx, NDF, NULL, ac, LIBMVL_NO_METADATA));

mvl_add_directory_entry(ctx, mvl_write_named_list_as_data_frame(ctx, L, NDF, LIBMVL_NULL_OFFSET), "df");

mvl_free_named_list(L);

mvl_add_directory_entry(ctx, mvl_write_string(ctx, -1, "example4.c", LIBMVL_NO_METADATA), "generated_by");

mvl_add_directory_entry(ctx, MVL_WVEC(ctx, LIBMVL_VECTOR_INT32, 1, 2, 3, 5, 7, 11, 13, 17), "primes");

/* Now memory map written out data and compute checksums */

fflush(fout);
length=ftell(fout);

data=mmap(NULL, length, PROT_READ, MAP_SHARED, fileno(fout), 0);
if(data==MAP_FAILED) {
	perror("Memory mapping test4.mvl");
	exit(-1);
	}

mvl_add_directory_entry(ctx, mvl_write_hash64_checksum_vector(ctx, data, 0, length, 65536), LIBMVL_FULL_CHECKSUMS_DIRECTORY_KEY);

mvl_close(ctx);
mvl_free_context(ctx);
fclose(fout);
fout=NULL;
exit(0);
}
