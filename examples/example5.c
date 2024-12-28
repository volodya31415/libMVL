#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "libMVL.h"

char *data;

int main(int argc, char *argv[])
{
FILE *fin;
long i;
LIBMVL_CONTEXT *ctx;
LIBMVL_NAMED_LIST *L;
LIBMVL_OFFSET64 length, offset_ad, offset_ac, offset_checksums;
LIBMVL_VECTOR *vec_ad, *vec_ac;


fin=fopen("test4.mvl", "r");
if(fin==NULL) {
	perror("test4.mvl");
	exit(-1);
	}
	
fseek(fin, 0, SEEK_END);
length=ftell(fin);
fseek(fin, 0, SEEK_SET);

data=mmap(NULL, length, PROT_READ, MAP_SHARED, fileno(fin), 0);
if(data==MAP_FAILED) {
	perror("Memory mapping test4.mvl");
	exit(-1);
	}
	
/* We don't need the handle anymore, free up the file descriptor */
fclose(fin);
fin=NULL;

ctx=mvl_create_context();
mvl_load_image(ctx, data, length);
	
if(mvl_verify_full_checksum_vector(ctx, NULL, NULL, 0)) {
	fprintf(stderr, "Error verifying full checksums: %s\n", mvl_strerror(ctx));
	exit(-1);
	}

L=mvl_read_named_list(ctx, NULL, 0, mvl_find_directory_entry(ctx, "df"));

offset_ad=mvl_find_list_entry(L, -1, "ad");
if(offset_ad==LIBMVL_NULL_OFFSET) {
	fprintf(stderr, "Could not find data frame member ad\n");
	exit(-1);
	}

/* Example how to verify single vector, if the file is too large to verify fully */
if(mvl_verify_checksum_vector2(ctx, NULL, NULL, 0, offset_ad)) {
	fprintf(stderr, "Error verifying vector checksums: %s\n", mvl_strerror(ctx));
	exit(-1);
	}
	
vec_ad=(LIBMVL_VECTOR *)&(data[offset_ad]);

/* Example how to verify a portion of a vector if it is too large to verify fully */
if(mvl_verify_checksum_vector3(ctx, NULL, NULL, 0, &(mvl_vector_data(vec_ad).d[100]), &(mvl_vector_data(vec_ad).d[120]))) {
	fprintf(stderr, "Error verifying data checksums: %s\n", mvl_strerror(ctx));
	exit(-1);
	}


offset_ac=mvl_find_list_entry(L, -1, "ac");
if(offset_ac==LIBMVL_NULL_OFFSET) {
	fprintf(stderr, "Could not find data frame member ac\n");
	exit(-1);
	}

	
vec_ac=(LIBMVL_VECTOR *)&(data[offset_ac]);

printf("ad\tac\n");
for(int i=100;i<120;i++) {
	printf("%lg\t", mvl_vector_data(vec_ad).d[i]);
	printf("%.*s\n", (int)mvl_packed_list_get_entry_bytelength(vec_ac, i), mvl_packed_list_get_entry(vec_ac, data, i)) ;
	}
fflush(stdout);

mvl_free_named_list(L);
mvl_free_context(ctx);

munmap(data, length);

exit(0);
}
