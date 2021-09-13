/*
    pdqidxsort.h - Pattern-defeating quicksort.

    Copyright (c) 2021 Orson Peters

    This software is provided 'as-is', without any express or implied warranty. In no event will the
    authors be held liable for any damages arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose, including commercial
    applications, and to alter it and redistribute it freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not claim that you wrote the
       original software. If you use this software in a product, an acknowledgment in the product
       documentation would be appreciated but is not required.

    2. Altered source versions must be plainly marked as such, and must not be misrepresented as
       being the original software.

    3. This notice may not be removed or altered from any source distribution.
    
    This file has been modified by Vladimir Dergachev (c) 2021 under the same terms as original (zlib license).
    
    The modification allows sorting of two arrays simultaneously, while maintaining row relationships. 
    The usual application is finding a permutation (i.e. indices) that would arrange an array in ascending or descending order.
    
    There are several ways to use regular pdqsort for this purpose, but this method is faster.
    
    In particular, the following methods are not optimal:
    
    1. using lambda function like [data] (i1, i2) { return data[i1]>data[i2] } 
       this results in considerable slowdown compared to other methods because of indirect reference and because accesses data[i1] end
       up being scattered, resulting in effectively much larger bandwidth requirements than other methods
       
    2. Using std::pair to couple elements of two arrays. This needs an auxiliary array the same length as input data, and also does not work well with caches and vector instructions.
    
    3. Using custom iterator class - an investigation of this for regular std::sort showed that the resulting method is slower
    
    
*/


#ifndef PDQIDXSORT_H
#define PDQIDXSORT_H

#include <algorithm>
#include <cstddef>
#include <functional>
#include <utility>
#include <iterator>

#if __cplusplus >= 201103L
    #include <cstdint>
    #include <type_traits>
    #define PDQIDXSORT_PREFER_MOVE(x) std::move(x)
#else
    #define PDQIDXSORT_PREFER_MOVE(x) (x)
#endif


namespace pdqidxsort_detail {
    enum {
        // Partitions below this size are sorted using insertion sort.
        insertion_sort_threshold = 24,

        // Partitions above this size use Tukey's ninther to select the pivot.
        ninther_threshold = 128,

        // When we detect an already sorted partition, attempt an insertion sort that allows this
        // amount of element moves before giving up.
        partial_insertion_sort_limit = 8,

        // Must be multiple of 8 due to loop unrolling, and < 256 to fit in unsigned char.
        block_size = 64,

        // Cacheline size, assumes power of two.
        cacheline_size = 64

    };

#if __cplusplus >= 201103L
    template<class T> struct is_default_compare : std::false_type { };
    template<class T> struct is_default_compare<std::less<T>> : std::true_type { };
    template<class T> struct is_default_compare<std::greater<T>> : std::true_type { };
#endif

    // Returns floor(log2(n)), assumes n > 0.
    template<class T>
    inline int log2(T n) {
        int log = 0;
        while (n >>= 1) ++log;
        return log;
    }

    // Sorts [begin, end) using insertion sort with the given comparison function.
    template<class Iter, class Iter2, class Compare>
    inline void insertion_sort(Iter begin, Iter end, Iter2 begin2, Compare comp) {
        typedef typename std::iterator_traits<Iter>::value_type T;
        typedef typename std::iterator_traits<Iter2>::value_type T2;
        if (begin == end) return;

        for (Iter cur = begin + 1; cur != end; ++cur) {
            Iter sift = cur;
            Iter sift_1 = cur - 1;

            // Compare first so we can avoid 2 moves for an element already positioned correctly.
            if (comp(*sift, *sift_1)) {
                T tmp = PDQIDXSORT_PREFER_MOVE(*sift);
                T2 tmp2 = PDQIDXSORT_PREFER_MOVE(*(begin2+(sift-begin)));

                do { 
			*sift = PDQIDXSORT_PREFER_MOVE(*sift_1); 
			*(begin2+(sift-begin)) = PDQIDXSORT_PREFER_MOVE(*(begin2+(sift_1-begin))); 
			
			sift--;
			
			}
                while (sift != begin && comp(tmp, *--sift_1));

                *sift = PDQIDXSORT_PREFER_MOVE(tmp);
                *(begin2+(sift-begin)) = PDQIDXSORT_PREFER_MOVE(tmp2);
            }
        }
    }

    // Sorts [begin, end) using insertion sort with the given comparison function. Assumes
    // *(begin - 1) is an element smaller than or equal to any element in [begin, end).
    template<class Iter, class Iter2, class Compare>
    inline void unguarded_insertion_sort(Iter begin, Iter end, Iter2 begin2, Compare comp) {
        typedef typename std::iterator_traits<Iter>::value_type T;
        typedef typename std::iterator_traits<Iter2>::value_type T2;
        if (begin == end) return;

        for (Iter cur = begin + 1; cur != end; ++cur) {
            Iter sift = cur;
            Iter sift_1 = cur - 1;

            // Compare first so we can avoid 2 moves for an element already positioned correctly.
            if (comp(*sift, *sift_1)) {
                T tmp = PDQIDXSORT_PREFER_MOVE(*sift);
                T2 tmp2 = PDQIDXSORT_PREFER_MOVE(*(begin2+(sift-begin)));

                do { 
			*sift = PDQIDXSORT_PREFER_MOVE(*sift_1); 
			*(begin2+(sift-begin)) = PDQIDXSORT_PREFER_MOVE(*(begin2+(sift_1-begin))); 
			sift--;
			
		}
                while (comp(tmp, *--sift_1));

                *sift = PDQIDXSORT_PREFER_MOVE(tmp);
                *(begin2+(sift-begin)) = PDQIDXSORT_PREFER_MOVE(tmp2);
            }
        }
    }

    // Attempts to use insertion sort on [begin, end). Will return false if more than
    // partial_insertion_sort_limit elements were moved, and abort sorting. Otherwise it will
    // successfully sort and return true.
    template<class Iter, class Iter2, class Compare>
    inline bool partial_insertion_sort(Iter begin, Iter end, Iter2 begin2, Compare comp) {
        typedef typename std::iterator_traits<Iter>::value_type T;
        typedef typename std::iterator_traits<Iter2>::value_type T2;
        if (begin == end) return true;
        
        std::size_t limit = 0;
        for (Iter cur = begin + 1; cur != end; ++cur) {
            Iter sift = cur;
            Iter sift_1 = cur - 1;

            // Compare first so we can avoid 2 moves for an element already positioned correctly.
            if (comp(*sift, *sift_1)) {
                T tmp = PDQIDXSORT_PREFER_MOVE(*sift);
                T2 tmp2 = PDQIDXSORT_PREFER_MOVE(*(begin2+(sift-begin)));

                do { 
			*sift = PDQIDXSORT_PREFER_MOVE(*sift_1); 
			*(begin2+(sift-begin))=PDQIDXSORT_PREFER_MOVE(*(begin2+(sift_1-begin)));
			sift--;
			
		}
                while (sift != begin && comp(tmp, *--sift_1));

                *sift = PDQIDXSORT_PREFER_MOVE(tmp);
                *(begin2+(sift-begin)) = PDQIDXSORT_PREFER_MOVE(tmp2);
                limit += cur - sift;
            }
            
            if (limit > partial_insertion_sort_limit) return false;
        }

        return true;
    }

    template<class Iter, class Iter2, class Compare>
    inline void sort2(Iter a, Iter b, Iter2 a2, Iter2 b2, Compare comp) {
        if (comp(*b, *a)) {
		std::iter_swap(a, b);
		std::iter_swap(a2, b2);
		}
    }

    // Sorts the elements *a, *b and *c using comparison function comp.
    template<class Iter, class Iter2, class Compare>
    inline void sort3(Iter a, Iter b, Iter c, Iter2 a2, Iter2 b2, Iter2 c2, Compare comp) {
        sort2(a, b, a2, b2, comp);
        sort2(b, c, b2, c2, comp);
        sort2(a, b, a2, b2, comp);
    }

    template<class T>
    inline T* align_cacheline(T* p) {
#if defined(UINTPTR_MAX) && __cplusplus >= 201103L
        std::uintptr_t ip = reinterpret_cast<std::uintptr_t>(p);
#else
        std::size_t ip = reinterpret_cast<std::size_t>(p);
#endif
        ip = (ip + cacheline_size - 1) & -cacheline_size;
        return reinterpret_cast<T*>(ip);
    }

    template<class Iter, class Iter2>
    inline void swap_offsets(Iter first, Iter last, Iter2 first2,
                             unsigned char* offsets_l, unsigned char* offsets_r,
                             size_t num, bool use_swaps) {
        typedef typename std::iterator_traits<Iter>::value_type T;
        typedef typename std::iterator_traits<Iter2>::value_type T2;
        if (use_swaps) {
            // This case is needed for the descending distribution, where we need
            // to have proper swapping for pdqidxsort to remain O(n).
            for (size_t i = 0; i < num; ++i) {
                std::iter_swap(first + offsets_l[i], last - offsets_r[i]);
                std::iter_swap(first2 + offsets_l[i], (first2+(last-first)) - offsets_r[i]);
            }
        } else if (num > 0) {
            Iter l = first + offsets_l[0]; Iter r = last - offsets_r[0];
            T tmp(PDQIDXSORT_PREFER_MOVE(*l)); 
            T2 tmp2(PDQIDXSORT_PREFER_MOVE(*(first2+(l-first)))); 
	    *l = PDQIDXSORT_PREFER_MOVE(*r);
	    *(first2+(l-first)) = PDQIDXSORT_PREFER_MOVE(*(first2+(r-first)));
            for (size_t i = 1; i < num; ++i) {
                l = first + offsets_l[i]; 
		*r = PDQIDXSORT_PREFER_MOVE(*l);
		*(first2+(r-first)) = PDQIDXSORT_PREFER_MOVE(*(first2+(l-first)));
                r = last - offsets_r[i]; 
		*l = PDQIDXSORT_PREFER_MOVE(*r);
		*(first2+(l-first)) = PDQIDXSORT_PREFER_MOVE(*(first2+(r-first)));
            }
            *r = PDQIDXSORT_PREFER_MOVE(tmp);
            *(first2+(r-first)) = PDQIDXSORT_PREFER_MOVE(tmp2);
        }
    }

    // Partitions [begin, end) around pivot *begin using comparison function comp. Elements equal
    // to the pivot are put in the right-hand partition. Returns the position of the pivot after
    // partitioning and whether the passed sequence already was correctly partitioned. Assumes the
    // pivot is a median of at least 3 elements and that [begin, end) is at least
    // insertion_sort_threshold long. Uses branchless partitioning.
    template<class Iter, class Iter2, class Compare>
    inline std::pair<Iter, bool> partition_right_branchless(Iter begin, Iter end, Iter2 begin2, Compare comp) {
        typedef typename std::iterator_traits<Iter>::value_type T;
        typedef typename std::iterator_traits<Iter2>::value_type T2;

        // Move pivot into local for speed.
        T pivot(PDQIDXSORT_PREFER_MOVE(*begin));
        T2 pivot2(PDQIDXSORT_PREFER_MOVE(*begin2));
        Iter first = begin;
        Iter last = end;

        // Find the first element greater than or equal than the pivot (the median of 3 guarantees
        // this exists).
        while (comp(*++first, pivot));

        // Find the first element strictly smaller than the pivot. We have to guard this search if
        // there was no element before *first.
        if (first - 1 == begin) while (first < last && !comp(*--last, pivot));
        else                    while (                !comp(*--last, pivot));

        // If the first pair of elements that should be swapped to partition are the same element,
        // the passed in sequence already was correctly partitioned.
        bool already_partitioned = first >= last;
        if (!already_partitioned) {
            std::iter_swap(first, last);
            std::iter_swap(begin2+(first-begin), begin2+(last-begin));
            ++first;

            // The following branchless partitioning is derived from "BlockQuicksort: How Branch
            // Mispredictions don’t affect Quicksort" by Stefan Edelkamp and Armin Weiss, but
            // heavily micro-optimized.
            unsigned char offsets_l_storage[block_size + cacheline_size];
            unsigned char offsets_r_storage[block_size + cacheline_size];
            unsigned char* offsets_l = align_cacheline(offsets_l_storage);
            unsigned char* offsets_r = align_cacheline(offsets_r_storage);

            Iter offsets_l_base = first;
            Iter offsets_r_base = last;
            size_t num_l, num_r, start_l, start_r;
            num_l = num_r = start_l = start_r = 0;
            
            while (first < last) {
                // Fill up offset blocks with elements that are on the wrong side.
                // First we determine how much elements are considered for each offset block.
                size_t num_unknown = last - first;
                size_t left_split = num_l == 0 ? (num_r == 0 ? num_unknown / 2 : num_unknown) : 0;
                size_t right_split = num_r == 0 ? (num_unknown - left_split) : 0;

                // Fill the offset blocks.
                if (left_split >= block_size) {
                    for (size_t i = 0; i < block_size;) {
                        offsets_l[num_l] = i++; num_l += !comp(*first, pivot); ++first;
                        offsets_l[num_l] = i++; num_l += !comp(*first, pivot); ++first;
                        offsets_l[num_l] = i++; num_l += !comp(*first, pivot); ++first;
                        offsets_l[num_l] = i++; num_l += !comp(*first, pivot); ++first;
                        offsets_l[num_l] = i++; num_l += !comp(*first, pivot); ++first;
                        offsets_l[num_l] = i++; num_l += !comp(*first, pivot); ++first;
                        offsets_l[num_l] = i++; num_l += !comp(*first, pivot); ++first;
                        offsets_l[num_l] = i++; num_l += !comp(*first, pivot); ++first;
                    }
                } else {
                    for (size_t i = 0; i < left_split;) {
                        offsets_l[num_l] = i++; num_l += !comp(*first, pivot); ++first;
                    }
                }

                if (right_split >= block_size) {
                    for (size_t i = 0; i < block_size;) {
                        offsets_r[num_r] = ++i; num_r += comp(*--last, pivot);
                        offsets_r[num_r] = ++i; num_r += comp(*--last, pivot);
                        offsets_r[num_r] = ++i; num_r += comp(*--last, pivot);
                        offsets_r[num_r] = ++i; num_r += comp(*--last, pivot);
                        offsets_r[num_r] = ++i; num_r += comp(*--last, pivot);
                        offsets_r[num_r] = ++i; num_r += comp(*--last, pivot);
                        offsets_r[num_r] = ++i; num_r += comp(*--last, pivot);
                        offsets_r[num_r] = ++i; num_r += comp(*--last, pivot);
                    }
                } else {
                    for (size_t i = 0; i < right_split;) {
                        offsets_r[num_r] = ++i; num_r += comp(*--last, pivot);
                    }
                }

                // Swap elements and update block sizes and first/last boundaries.
                size_t num = std::min(num_l, num_r);
                swap_offsets(offsets_l_base, offsets_r_base, begin2+(offsets_l_base-begin),
                             offsets_l + start_l, offsets_r + start_r,
                             num, num_l == num_r);
                num_l -= num; num_r -= num;
                start_l += num; start_r += num;

                if (num_l == 0) {
                    start_l = 0;
                    offsets_l_base = first;
                }
                
                if (num_r == 0) {
                    start_r = 0;
                    offsets_r_base = last;
                }
            }

            // We have now fully identified [first, last)'s proper position. Swap the last elements.
            if (num_l) {
                offsets_l += start_l;
                while (num_l--) {
			--last;
			std::iter_swap(offsets_l_base + offsets_l[num_l], last);
			std::iter_swap(begin2+(offsets_l_base + offsets_l[num_l]-begin), begin2+(last-begin));
			}
                first = last;
            }
            if (num_r) {
                offsets_r += start_r;
                while (num_r--) {
			std::iter_swap(offsets_r_base - offsets_r[num_r], first);
			std::iter_swap(begin2+(offsets_r_base - offsets_r[num_r]-begin), begin2+(first-begin));
			++first;
			}
                last = first;
            }
        }

        // Put the pivot in the right place.
        Iter pivot_pos = first - 1;
        *begin = PDQIDXSORT_PREFER_MOVE(*pivot_pos);
        *begin2 = PDQIDXSORT_PREFER_MOVE(*(begin2+(pivot_pos-begin)));
        *pivot_pos = PDQIDXSORT_PREFER_MOVE(pivot);
        *(begin2+(pivot_pos-begin)) = PDQIDXSORT_PREFER_MOVE(pivot2);

        return std::make_pair(pivot_pos, already_partitioned);
    }



    // Partitions [begin, end) around pivot *begin using comparison function comp. Elements equal
    // to the pivot are put in the right-hand partition. Returns the position of the pivot after
    // partitioning and whether the passed sequence already was correctly partitioned. Assumes the
    // pivot is a median of at least 3 elements and that [begin, end) is at least
    // insertion_sort_threshold long.
    template<class Iter, class Iter2, class Compare>
    inline std::pair<Iter, bool> partition_right(Iter begin, Iter end, Iter2 begin2, Compare comp) {
        typedef typename std::iterator_traits<Iter>::value_type T;
        typedef typename std::iterator_traits<Iter2>::value_type T2;
        
        // Move pivot into local for speed.
        T pivot(PDQIDXSORT_PREFER_MOVE(*begin));
        T2 pivot2(PDQIDXSORT_PREFER_MOVE(*begin2));

        Iter first = begin;
        Iter last = end;

        // Find the first element greater than or equal than the pivot (the median of 3 guarantees
        // this exists).
        while (comp(*++first, pivot));

        // Find the first element strictly smaller than the pivot. We have to guard this search if
        // there was no element before *first.
        if (first - 1 == begin) while (first < last && !comp(*--last, pivot));
        else                    while (                !comp(*--last, pivot));

        // If the first pair of elements that should be swapped to partition are the same element,
        // the passed in sequence already was correctly partitioned.
        bool already_partitioned = first >= last;
        
        // Keep swapping pairs of elements that are on the wrong side of the pivot. Previously
        // swapped pairs guard the searches, which is why the first iteration is special-cased
        // above.
        while (first < last) {
            std::iter_swap(first, last);
            std::iter_swap(begin2+(first-begin), begin2+(last-begin));
            while (comp(*++first, pivot));
            while (!comp(*--last, pivot));
        }

        // Put the pivot in the right place.
        Iter pivot_pos = first - 1;
        *begin = PDQIDXSORT_PREFER_MOVE(*pivot_pos);
        *begin2 = PDQIDXSORT_PREFER_MOVE(*(begin2+(pivot_pos-begin)));
        *pivot_pos = PDQIDXSORT_PREFER_MOVE(pivot);
        *(begin2+(pivot_pos-begin)) = PDQIDXSORT_PREFER_MOVE(pivot2);

        return std::make_pair(pivot_pos, already_partitioned);
    }

    // Similar function to the one above, except elements equal to the pivot are put to the left of
    // the pivot and it doesn't check or return if the passed sequence already was partitioned.
    // Since this is rarely used (the many equal case), and in that case pdqidxsort already has O(n)
    // performance, no block quicksort is applied here for simplicity.
    template<class Iter, class Iter2, class Compare>
    inline Iter partition_left(Iter begin, Iter end, Iter2 begin2, Compare comp) {
        typedef typename std::iterator_traits<Iter>::value_type T;
        typedef typename std::iterator_traits<Iter2>::value_type T2;

        T pivot(PDQIDXSORT_PREFER_MOVE(*begin));
        T2 pivot2(PDQIDXSORT_PREFER_MOVE(*begin2));
        Iter first = begin;
        Iter last = end;
        
        while (comp(pivot, *--last));

        if (last + 1 == end) while (first < last && !comp(pivot, *++first));
        else                 while (                !comp(pivot, *++first));

        while (first < last) {
            std::iter_swap(first, last);
            std::iter_swap(begin2+(first-begin), begin2+(last-begin));
            while (comp(pivot, *--last));
            while (!comp(pivot, *++first));
        }

        Iter pivot_pos = last;
        *begin = PDQIDXSORT_PREFER_MOVE(*pivot_pos);
        *begin2 = PDQIDXSORT_PREFER_MOVE(*(begin2+(pivot_pos-begin)));
        *pivot_pos = PDQIDXSORT_PREFER_MOVE(pivot);
        *(begin2+(pivot_pos-begin)) = PDQIDXSORT_PREFER_MOVE(pivot2);

        return pivot_pos;
    }


    template<class Iter, class Iter2, class Compare, bool Branchless>
    inline void pdqidxsort_loop(Iter begin, Iter end, Iter2 begin2, Compare comp, int bad_allowed, bool leftmost = true) {
        typedef typename std::iterator_traits<Iter>::difference_type diff_t;

        // Use a while loop for tail recursion elimination.
        while (true) {
            diff_t size = end - begin;

            // Insertion sort is faster for small arrays.
            if (size < insertion_sort_threshold) {
                if (leftmost) insertion_sort(begin, end, begin2, comp);
                else unguarded_insertion_sort(begin, end, begin2, comp);
                return;
            }

            // Choose pivot as median of 3 or pseudomedian of 9.
            diff_t s2 = size / 2;
            if (size > ninther_threshold) {
                sort3(begin, begin + s2, end - 1, begin2, begin2+s2, begin2+(end-1-begin), comp);
                sort3(begin + 1, begin + (s2 - 1), end - 2, begin2+1, begin2+(s2-1), begin2+(end-2-begin), comp);
                sort3(begin + 2, begin + (s2 + 1), end - 3, begin2+2, begin2+(s2+1), begin2+(end-3-begin), comp);
                sort3(begin + (s2 - 1), begin + s2, begin + (s2 + 1), begin2+(s2-1), begin2+s2, begin2+(s2+1), comp);
                std::iter_swap(begin, begin + s2);
                std::iter_swap(begin2, begin2 + s2);
            } else sort3(begin + s2, begin, end - 1, begin2+s2, begin2, begin2+(end-1-begin), comp);

            // If *(begin - 1) is the end of the right partition of a previous partition operation
            // there is no element in [begin, end) that is smaller than *(begin - 1). Then if our
            // pivot compares equal to *(begin - 1) we change strategy, putting equal elements in
            // the left partition, greater elements in the right partition. We do not have to
            // recurse on the left partition, since it's sorted (all equal).
            if (!leftmost && !comp(*(begin - 1), *begin)) {
                Iter beginA = partition_left(begin, end, begin2, comp) + 1;
		begin2=begin2+(beginA-begin);
		begin=beginA;
                continue;
            }

            // Partition and get results.
            std::pair<Iter, bool> part_result =
                Branchless ? partition_right_branchless(begin, end, begin2, comp)
                           : partition_right(begin, end, begin2, comp);
            Iter pivot_pos = part_result.first;
            bool already_partitioned = part_result.second;

            // Check for a highly unbalanced partition.
            diff_t l_size = pivot_pos - begin;
            diff_t r_size = end - (pivot_pos + 1);
            bool highly_unbalanced = l_size < size / 8 || r_size < size / 8;

            // If we got a highly unbalanced partition we shuffle elements to break many patterns.
            if (highly_unbalanced) {
                // If we had too many bad partitions, switch to heapsort to guarantee O(n log n).
                if (0 && --bad_allowed == 0) {
		    /** TODO: convert std::make_heap */
                    std::make_heap(begin, end, comp);
                    std::sort_heap(begin, end, comp);
                    return;
                }

                if (l_size >= insertion_sort_threshold) {
                    std::iter_swap(begin,             begin + l_size / 4);
                    std::iter_swap(begin2,             begin2 + l_size / 4);
                    std::iter_swap(pivot_pos - 1, pivot_pos - l_size / 4);
                    std::iter_swap(begin2+(pivot_pos - 1-begin), begin2+(pivot_pos - l_size / 4-begin));

                    if (l_size > ninther_threshold) {
                        std::iter_swap(begin + 1,         begin + (l_size / 4 + 1));
                        std::iter_swap(begin2 + 1,         begin2 + (l_size / 4 + 1));

			std::iter_swap(begin + 2,         begin + (l_size / 4 + 2));
			std::iter_swap(begin2 + 2,         begin2 + (l_size / 4 + 2));
			
                        std::iter_swap(pivot_pos - 2, pivot_pos - (l_size / 4 + 1));
                        std::iter_swap(begin2+(pivot_pos - 2-begin), begin2+(pivot_pos - (l_size / 4 + 1)-begin));
			
                        std::iter_swap(pivot_pos - 3, pivot_pos - (l_size / 4 + 2));
                        std::iter_swap(begin2+(pivot_pos - 3-begin), begin2+(pivot_pos - (l_size / 4 + 2)-begin));
                    }
                }
                
                if (r_size >= insertion_sort_threshold) {
                    std::iter_swap(pivot_pos + 1, pivot_pos + (1 + r_size / 4));
                    std::iter_swap(begin2+(pivot_pos + 1-begin), begin2+(pivot_pos + (1 + r_size / 4)-begin));
		    
                    std::iter_swap(end - 1,                   end - r_size / 4);
                    std::iter_swap(begin2+(end - 1-begin),                   begin2+(end - r_size / 4-begin));
                    
                    if (r_size > ninther_threshold) {
                        std::iter_swap(pivot_pos + 2, pivot_pos + (2 + r_size / 4));
                        std::iter_swap(begin2+(pivot_pos + 2-begin), begin2+(pivot_pos + (2 + r_size / 4)-begin));
			
                        std::iter_swap(pivot_pos + 3, pivot_pos + (3 + r_size / 4));
                        std::iter_swap(begin2+(pivot_pos + 3-begin), begin2+(pivot_pos + (3 + r_size / 4)-begin));

			std::iter_swap(end - 2,             end - (1 + r_size / 4));
			std::iter_swap(begin2+(end - 2-begin),             begin2+(end - (1 + r_size / 4)-begin));
			
                        std::iter_swap(end - 3,             end - (2 + r_size / 4));
                        std::iter_swap(begin2+(end - 3-begin),             begin2+(end - (2 + r_size / 4)-begin));
                    }
                }
            } else {
                // If we were decently balanced and we tried to sort an already partitioned
                // sequence try to use insertion sort.
                if (already_partitioned && partial_insertion_sort(begin, pivot_pos, begin2, comp)
                                        && partial_insertion_sort(pivot_pos + 1, end, begin2+(pivot_pos+1-begin), comp)) return;
            }
                
            // Sort the left partition first using recursion and do tail recursion elimination for
            // the right-hand partition.
            pdqidxsort_loop<Iter, Iter2, Compare, Branchless>(begin, pivot_pos, begin2, comp, bad_allowed, leftmost);
	    begin2=begin2+(pivot_pos+1-begin);
            begin = pivot_pos + 1;
            leftmost = false;
        }
    }
}


template<class Iter, class Iter2, class Compare>
inline void pdqidxsort(Iter begin, Iter end, Iter2 begin2, Compare comp) {
    if (begin == end) return;

#if __cplusplus >= 201103L
    pdqidxsort_detail::pdqidxsort_loop<Iter, Iter2, Compare,
        pdqidxsort_detail::is_default_compare<typename std::decay<Compare>::type>::value &&
        std::is_arithmetic<typename std::iterator_traits<Iter>::value_type>::value>(
        begin, end, begin2, comp, pdqidxsort_detail::log2(end - begin));
#else
    pdqidxsort_detail::pdqidxsort_loop<Iter, Iter2, Compare, false>(
        begin, end, begin2, comp, pdqidxsort_detail::log2(end - begin));
#endif
}

template<class Iter, class Iter2>
inline void pdqidxsort(Iter begin, Iter end, Iter2 begin2) {
    typedef typename std::iterator_traits<Iter>::value_type T;
    pdqidxsort(begin, end, begin2, std::less<T>());
}

template<class Iter, class Iter2, class Compare>
inline void pdqidxsort_branchless(Iter begin, Iter end, Iter2 begin2, Compare comp) {
    if (begin == end) return;
    pdqidxsort_detail::pdqidxsort_loop<Iter, Iter2, Compare, true>(
        begin, end, begin2, comp, pdqidxsort_detail::log2(end - begin));
}

template<class Iter, class Iter2>
inline void pdqidxsort_branchless(Iter begin, Iter end, Iter2 begin2) {
    typedef typename std::iterator_traits<Iter>::value_type T;
    pdqidxsort_branchless(begin, end, begin2, std::less<T>());
}


#undef PDQIDXSORT_PREFER_MOVE

#endif
