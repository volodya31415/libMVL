#include <algorithm>
#include <vector>
#include <functional>
#include "pdqsort.h"
#include "pdqidxsort.h"
#include "libMVL.h"


template <class Numeric> 
static void sort_indices_asc1(LIBMVL_OFFSET64 start, LIBMVL_OFFSET64 stop, LIBMVL_OFFSET64 *indices, Numeric *data)
{
//std::vector<std::reference_wrapper<LIBMVL_OFFSET64>> indices_pp(indices+start, indices+stop);
//std::vector<LIBMVL_OFFSET64> indices_pp(indices+start, indices+stop);

std::sort(indices+start, indices+stop, [data](LIBMVL_OFFSET64 i1, LIBMVL_OFFSET64 i2) { return(data[i1]<data[i2]); });
}

template <class Numeric> 
static void sort_indices_asc2(LIBMVL_OFFSET64 start, LIBMVL_OFFSET64 stop, LIBMVL_OFFSET64 *indices, Numeric *data)
{
std::vector<std::pair<Numeric, LIBMVL_OFFSET64>> indices_pp;

indices_pp.reserve(stop-start);
for(LIBMVL_OFFSET64 i=start;i<stop;i++) {
	indices_pp[i-start]=std::make_pair(data[indices[i]], indices[i]); 
	}

std::sort(indices_pp.begin(), indices_pp.begin()+(stop-start), [](std::pair<Numeric, LIBMVL_OFFSET64> i1, std::pair<Numeric, LIBMVL_OFFSET64> i2) { return (i1.first<i2.first);});

for(LIBMVL_OFFSET64 i=start;i<stop;i++) {
	indices[i]=indices_pp[i-start].second; 
	}
}

// template <class Numeric> 
// static void sort_indices_asc3(LIBMVL_OFFSET64 start, LIBMVL_OFFSET64 stop, LIBMVL_OFFSET64 *indices, Numeric *data)
// {
// std::vector<Numeric> values(data+start, data+stop);
// 
// /* TODO : pairs */
// 
// std::sort(indices_pp.begin(), indices_pp.begin()+(stop-start), [](std::pair<Numeric, LIBMVL_OFFSET64> i1, std::pair<Numeric, LIBMVL_OFFSET64> i2) { return (i1.first<i2.first);});
// 
// }

template <class Numeric> 
static void sort_indices_asc4(LIBMVL_OFFSET64 start, LIBMVL_OFFSET64 stop, LIBMVL_OFFSET64 *indices, Numeric *data)
{
std::vector<std::pair<Numeric, LIBMVL_OFFSET64>> indices_pp;

indices_pp.reserve(stop-start);
for(LIBMVL_OFFSET64 i=start;i<stop;i++) {
	indices_pp[i-start]=std::make_pair(data[indices[i]], indices[i]); 
	}

pdqsort(indices_pp.begin(), indices_pp.begin()+(stop-start), [](std::pair<Numeric, LIBMVL_OFFSET64> i1, std::pair<Numeric, LIBMVL_OFFSET64> i2) { return (i1.first<i2.first);});

for(LIBMVL_OFFSET64 i=start;i<stop;i++) {
	indices[i]=indices_pp[i-start].second; 
	}
}

template <class Numeric> 
static void sort_indices_asc(LIBMVL_OFFSET64 count, LIBMVL_OFFSET64 *indices, Numeric *data)
{

pdqidxsort_branchless(data, data+count, indices, [](Numeric a, Numeric b) { return (a<b);});

// for(int i=0;i<stop-start;i++) {
// 	if(values[i]!=data[indices[i+start]])
// 		fprintf(stderr, "%d %.12g %.12g %lld\n", i, values[i], data[indices[i+start]], indices[i+start]);
// 	}

// for(int i=0;i<stop-start-1;i++) {
// 	if(values[i]>values[i+1])fprintf(stderr, "%d %g %g\n", i, values[i], values[i+1]);
// 	if(data[indices[i+start]]>data[indices[i+1+start]])fprintf(stderr, "%d %g %g %lld %lld\n", i, data[indices[i+start]], data[indices[i+1+start]], indices[i+1], indices[i+1+start]);
// 	}
}

template <class Numeric> 
static void sort_indices_desc(LIBMVL_OFFSET64 count, LIBMVL_OFFSET64 *indices, Numeric *data)
{

pdqidxsort_branchless(data, data+count, indices, [](Numeric a, Numeric b) { return (a>b);});

// for(int i=0;i<stop-start;i++) {
// 	if(values[i]!=data[indices[i+start]])
// 		fprintf(stderr, "%d %.12g %.12g %lld\n", i, values[i], data[indices[i+start]], indices[i+start]);
// 	}

// for(int i=0;i<stop-start-1;i++) {
// 	if(values[i]>values[i+1])fprintf(stderr, "%d %g %g\n", i, values[i], values[i+1]);
// 	if(data[indices[i+start]]>data[indices[i+1+start]])fprintf(stderr, "%d %g %g %lld %lld\n", i, data[indices[i+start]], data[indices[i+1+start]], indices[i+1], indices[i+1+start]);
// 	}
}

static void sort_indices_packed_list64_asc(LIBMVL_OFFSET64 start, LIBMVL_OFFSET64 stop, LIBMVL_OFFSET64 *indices, LIBMVL_VECTOR *vec, void *data)
{
std::sort(indices+start, indices+stop, [vec, data](LIBMVL_OFFSET64 i1, LIBMVL_OFFSET64 i2) { 
	LIBMVL_OFFSET64 al, bl, nn;
	const char *ad, *bd;
	al=mvl_packed_list_get_entry_bytelength(vec, i1);
	bl=mvl_packed_list_get_entry_bytelength(vec, i2);
	ad=mvl_packed_list_get_entry(vec, data, i1);
	bd=mvl_packed_list_get_entry(vec, data, i2);
	nn=al;
	if(bl<nn)nn=bl;
	for(LIBMVL_OFFSET64 j=0;j<nn;j++) {
		if(ad[j]<bd[j])return true;
		if(ad[j]>bd[j])return false;
		}
	return(al<bl);
	});
}

static void sort_indices_packed_list64_desc(LIBMVL_OFFSET64 start, LIBMVL_OFFSET64 stop, LIBMVL_OFFSET64 *indices, LIBMVL_VECTOR *vec, void *data)
{
std::sort(indices+start, indices+stop, [vec, data](LIBMVL_OFFSET64 i1, LIBMVL_OFFSET64 i2) { 
	LIBMVL_OFFSET64 al, bl, nn;
	const char *ad, *bd;
	al=mvl_packed_list_get_entry_bytelength(vec, i1);
	bl=mvl_packed_list_get_entry_bytelength(vec, i2);
	ad=mvl_packed_list_get_entry(vec, data, i1);
	bd=mvl_packed_list_get_entry(vec, data, i2);
	nn=al;
	if(bl<nn)nn=bl;
	for(LIBMVL_OFFSET64 j=0;j<nn;j++) {
		if(ad[j]>bd[j])return true;
		if(ad[j]<bd[j])return false;
		}
	return(al>bl);
	});
}

template <class Numeric>
void mvl_find_ties(LIBMVL_OFFSET64 start, LIBMVL_OFFSET64 stop, Numeric *data, std::vector<std::pair<LIBMVL_OFFSET64, LIBMVL_OFFSET64>> &ties)
{
LIBMVL_OFFSET64 i,j;
i=0;
while(i<stop-start-1) {
	if(data[i]!=data[i+1]) {
		i++;
		continue;
		}
	for(j=i+2;(j<stop-start) && data[j]==data[i];j++);
	ties.push_back(std::make_pair(i+start, j+start));
	i=j;
	}
}


extern "C" {

void mvl_indexed_sort_single_vector_asc(LIBMVL_OFFSET64 start, LIBMVL_OFFSET64 stop, LIBMVL_OFFSET64 *indices, LIBMVL_VECTOR *vec, void *data, std::vector<char> &scratch)
{

switch(mvl_vector_type(vec)) {
	case LIBMVL_VECTOR_UINT8:
	case LIBMVL_VECTOR_CSTRING: {
		scratch.reserve((stop-start)*mvl_element_size(mvl_vector_type(vec)));
		unsigned char *b=mvl_vector_data(vec).b;
		unsigned char *d=(unsigned char*)scratch.data();
		for(LIBMVL_OFFSET64 i=0;i<stop-start;i++)d[i]=b[indices[i+start]];
		sort_indices_asc(stop-start, indices+start, d);
		break;
		}
	case LIBMVL_VECTOR_INT32: {
		scratch.reserve((stop-start)*mvl_element_size(mvl_vector_type(vec)));
		int *b=mvl_vector_data(vec).i;
		int *d=(int *)scratch.data();
		for(LIBMVL_OFFSET64 i=0;i<stop-start;i++)d[i]=b[indices[i+start]];
		sort_indices_asc(stop-start, indices+start, d);
		break;
		}
	case LIBMVL_VECTOR_FLOAT: {
		scratch.reserve((stop-start)*mvl_element_size(mvl_vector_type(vec)));
		float *b=mvl_vector_data(vec).f;
		float *d=(float *)scratch.data();
		for(LIBMVL_OFFSET64 i=0;i<stop-start;i++)d[i]=b[indices[i+start]];
		sort_indices_asc(stop-start, indices+start, d);
		break;
		}
	case LIBMVL_VECTOR_INT64: {
		scratch.reserve((stop-start)*mvl_element_size(mvl_vector_type(vec)));
		long long int *b=mvl_vector_data(vec).i64;
		long long int *d=(long long int*)scratch.data();
		for(LIBMVL_OFFSET64 i=0;i<stop-start;i++)d[i]=b[indices[i+start]];
		sort_indices_asc(stop-start, indices+start, d);
		break;
		}
	case LIBMVL_VECTOR_OFFSET64: {
		scratch.reserve((stop-start)*mvl_element_size(mvl_vector_type(vec)));
		LIBMVL_OFFSET64 *b=mvl_vector_data(vec).offset;
		LIBMVL_OFFSET64 *d=(LIBMVL_OFFSET64*)scratch.data();
		for(LIBMVL_OFFSET64 i=0;i<stop-start;i++)d[i]=b[indices[i+start]];
		sort_indices_asc(stop-start, indices+start, d);
		break;
		}
	case LIBMVL_VECTOR_DOUBLE: {
		scratch.reserve((stop-start)*mvl_element_size(mvl_vector_type(vec)));
		double *b=mvl_vector_data(vec).d;
		double *d=(double *)scratch.data();
		for(LIBMVL_OFFSET64 i=0;i<stop-start;i++)d[i]=b[indices[i+start]];
		sort_indices_asc(stop-start, indices+start, d);
		break;
		}
	case LIBMVL_PACKED_LIST64:
		sort_indices_packed_list64_asc(start, stop, indices, vec, data);
		break;
	default:
		break;
	}
}

void mvl_indexed_sort_single_vector_desc(LIBMVL_OFFSET64 start, LIBMVL_OFFSET64 stop, LIBMVL_OFFSET64 *indices, LIBMVL_VECTOR *vec, void *data, std::vector<char> &scratch)
{

switch(mvl_vector_type(vec)) {
	case LIBMVL_VECTOR_UINT8:
	case LIBMVL_VECTOR_CSTRING: {
		scratch.reserve((stop-start)*mvl_element_size(mvl_vector_type(vec)));
		unsigned char *b=mvl_vector_data(vec).b;
		unsigned char *d=(unsigned char*)scratch.data();
		for(LIBMVL_OFFSET64 i=0;i<stop-start;i++)d[i]=b[indices[i+start]];
		sort_indices_desc(stop-start, indices+start, d);
		break;
		}
	case LIBMVL_VECTOR_INT32: {
		scratch.reserve((stop-start)*mvl_element_size(mvl_vector_type(vec)));
		int *b=mvl_vector_data(vec).i;
		int *d=(int *)scratch.data();
		for(LIBMVL_OFFSET64 i=0;i<stop-start;i++)d[i]=b[indices[i+start]];
		sort_indices_desc(stop-start, indices+start, d);
		break;
		}
	case LIBMVL_VECTOR_FLOAT: {
		scratch.reserve((stop-start)*mvl_element_size(mvl_vector_type(vec)));
		float *b=mvl_vector_data(vec).f;
		float *d=(float *)scratch.data();
		for(LIBMVL_OFFSET64 i=0;i<stop-start;i++)d[i]=b[indices[i+start]];
		sort_indices_desc(stop-start, indices+start, d);
		break;
		}
	case LIBMVL_VECTOR_INT64: {
		scratch.reserve((stop-start)*mvl_element_size(mvl_vector_type(vec)));
		long long int *b=mvl_vector_data(vec).i64;
		long long int *d=(long long int*)scratch.data();
		for(LIBMVL_OFFSET64 i=0;i<stop-start;i++)d[i]=b[indices[i+start]];
		sort_indices_desc(stop-start, indices+start, d);
		break;
		}
	case LIBMVL_VECTOR_OFFSET64: {
		scratch.reserve((stop-start)*mvl_element_size(mvl_vector_type(vec)));
		LIBMVL_OFFSET64 *b=mvl_vector_data(vec).offset;
		LIBMVL_OFFSET64 *d=(LIBMVL_OFFSET64*)scratch.data();
		for(LIBMVL_OFFSET64 i=0;i<stop-start;i++)d[i]=b[indices[i+start]];
		sort_indices_desc(stop-start, indices+start, d);
		break;
		}
	case LIBMVL_VECTOR_DOUBLE: {
		scratch.reserve((stop-start)*mvl_element_size(mvl_vector_type(vec)));
		double *b=mvl_vector_data(vec).d;
		double *d=(double *)scratch.data();
		for(LIBMVL_OFFSET64 i=0;i<stop-start;i++)d[i]=b[indices[i+start]];
		sort_indices_desc(stop-start, indices+start, d);
		break;
		}
	case LIBMVL_PACKED_LIST64:
		sort_indices_packed_list64_desc(start, stop, indices, vec, data);
		break;
	default:
		break;
	}
}


static inline int mvl_packed64_equal(LIBMVL_VECTOR *vec, void *data, LIBMVL_OFFSET64 i1, LIBMVL_OFFSET64 i2)
{
LIBMVL_OFFSET64 al, bl;
const char *ad, *bd;
al=mvl_packed_list_get_entry_bytelength(vec, i1);
bl=mvl_packed_list_get_entry_bytelength(vec, i2);
if(al!=bl) {
	return 0;
	}
ad=mvl_packed_list_get_entry(vec, data, i1);
bd=mvl_packed_list_get_entry(vec, data, i2);
for(LIBMVL_OFFSET64 k=0;k<al;k++) {
	if(ad[k]!=bd[k])return 0;
	}
return 1;
}

void mvl_indexed_find_ties(LIBMVL_OFFSET64 start, LIBMVL_OFFSET64 stop, LIBMVL_OFFSET64 *indices, LIBMVL_VECTOR *vec, void *data, std::vector<char> &scratch, std::vector<std::pair<LIBMVL_OFFSET64, LIBMVL_OFFSET64>> &ties)
{

switch(mvl_vector_type(vec)) {
	case LIBMVL_VECTOR_UINT8:
	case LIBMVL_VECTOR_CSTRING: {
		unsigned char *d=(unsigned char*)scratch.data();
		mvl_find_ties(start, stop, d, ties);
		break;
		}
	case LIBMVL_VECTOR_INT32: {
		int *d=(int *)scratch.data();
		mvl_find_ties(start, stop, d, ties);
		break;
		}
	case LIBMVL_VECTOR_FLOAT: {
		float *d=(float *)scratch.data();
		mvl_find_ties(start, stop, d, ties);
		break;
		}
	case LIBMVL_VECTOR_INT64: {
		long long int *d=(long long int*)scratch.data();
		mvl_find_ties(start, stop, d, ties);
		break;
		}
	case LIBMVL_VECTOR_OFFSET64: {
		LIBMVL_OFFSET64 *d=(LIBMVL_OFFSET64*)scratch.data();
		mvl_find_ties(start, stop, d, ties);
		break;
		}
	case LIBMVL_VECTOR_DOUBLE: {
		double *d=(double *)scratch.data();
		mvl_find_ties(start, stop, d, ties);
		break;
		}
	case LIBMVL_PACKED_LIST64:
		LIBMVL_OFFSET64 i,j;
		i=start;
		while(i<stop-1) {
			if(!mvl_packed64_equal(vec, data, indices[i], indices[i+1])) {
				i++;
				continue;
				}
				
			for(j=i+2;j<stop && mvl_packed64_equal(vec, data, indices[i], indices[j]);j++);
			ties.push_back(std::make_pair(i, j));
			i=j;
			}
		break;
	default:
		break;
	}
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
 * This function return 0 on successful sort. If no vectors are supplies (vec_count==0) the indices are unchanged the sort is considered successful
 */
int mvl_sort_indices(LIBMVL_OFFSET64 indices_count, LIBMVL_OFFSET64 *indices, LIBMVL_OFFSET64 vec_count, LIBMVL_VECTOR **vec, void **vec_data, int sort_function)
{
if(vec_count<1)return 0;
LIBMVL_OFFSET64 i, j;

std::vector<char> scratch;
std::vector<std::pair<LIBMVL_OFFSET64, LIBMVL_OFFSET64>> ties1, ties2;


ties1.clear();
ties1.push_back(std::make_pair(0, indices_count));
	
for(i=0;i<vec_count;i++) {
	ties2.clear();
	for(j=0;j<ties1.size();j++) {
		switch(sort_function) {
			case LIBMVL_SORT_LEXICOGRAPHIC:
				mvl_indexed_sort_single_vector_asc(ties1[j].first, ties1[j].second, indices, vec[i], vec_data[i], scratch);
				break;
			case LIBMVL_SORT_LEXICOGRAPHIC_DESC:
				mvl_indexed_sort_single_vector_desc(ties1[j].first, ties1[j].second, indices, vec[i], vec_data[i], scratch);
				break;
			default:
				return -1;
			}
		
		mvl_indexed_find_ties(ties1[j].first, ties1[j].second, indices, vec[i], vec_data[i], scratch, ties2);
		}
	std::swap(ties1, ties2);
	if(ties1.size()<1)break;
	}
	
if(ties1.size()>0) {
	/* Sort indices in ascending order for any remaining ties. 
	 * This is important to improve locality of memory accesses */
	
	for(j=0;j<ties1.size();j++) {
		pdqsort(indices+ties1[j].first, indices+ties1[j].second);
		}
	}

return 0;
}

}
